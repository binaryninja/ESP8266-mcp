#include <string.h>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "tcpip_adapter.h"

// C++ MCP server includes
#include "MCPServer.h"
#include "EspSocketTransport.h"

static const char *TAG = "ESP8266-MCP";

// WiFi configuration - modify these for your network
#define WIFI_SSID      "FBI Surveillance Van"
#define WIFI_PASS      "jerjushanben2135"
#define WIFI_MAXIMUM_RETRY  5

// Server configuration
#define SERVER_PORT    8080

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

void print_memory_info(const char* location) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    // Check stack usage for current task
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(current_task);
    
    ESP_LOGI(TAG, "[%s] Free heap: %u bytes, Min free: %u bytes, Stack remaining: %u bytes", 
             location, (unsigned int)free_heap, (unsigned int)min_free_heap, 
             (unsigned int)(stack_high_water_mark * sizeof(StackType_t)));
}


extern "C" {
static esp_err_t event_handler(void* arg, system_event_t* event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            if (s_retry_num < WIFI_MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP");
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(TAG, "connect to the AP fail");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}
} // extern "C"

void init_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
        // For this demo, we'll restart if WiFi fails
        esp_restart();
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void mcp_server_task(void *pvParameters)
{
    print_memory_info("MCP task start");
    
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Set socket to reuse address
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_fd, 1) != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TinyMCP server listening on port %d", SERVER_PORT);

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_sock = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        ESP_LOGI(TAG, "Client connected from %s:%d", 
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        print_memory_info("Before MCP server creation");
        
        // Check stack usage before creating MCP server objects
        UBaseType_t stack_before = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Stack remaining before MCP server: %u bytes", 
                 (unsigned int)(stack_before * sizeof(StackType_t)));
        
        // Create C++ MCP server with socket transport
        auto transport = std::make_unique<tinymcp::EspSocketTransport>(client_sock);
        
        // Check stack usage after transport creation
        UBaseType_t stack_after_transport = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Stack remaining after transport creation: %u bytes", 
                 (unsigned int)(stack_after_transport * sizeof(StackType_t)));
        
        tinymcp::MCPServer server(transport.get());
        
        // Check stack usage after server creation
        UBaseType_t stack_after_server = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Stack remaining after server creation: %u bytes", 
                 (unsigned int)(stack_after_server * sizeof(StackType_t)));
        
        ESP_LOGI(TAG, "Starting C++ MCP server for client");
        server.run();  // This blocks until client disconnects
        ESP_LOGI(TAG, "Client disconnected");
        
        // Transport will be automatically destroyed here
        
        print_memory_info("After client disconnect");
        // Socket is closed by transport destructor
    }

    close(listen_fd);
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "ESP8266-MCP starting up...");
    print_memory_info("App start");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    print_memory_info("After NVS init");

    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    init_wifi();
    print_memory_info("After WiFi init");

    // Set WiFi power save to none for better performance
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "WiFi connected, starting MCP server...");

    // Create MCP server task with 8KB stack (increased for C++ stack usage)
    xTaskCreatePinnedToCore(
        mcp_server_task,
        "mcp_server",
        8192,           // Stack size (8KB - increased for C++ stack usage)
        NULL,           // Parameters
        5,              // Priority
        NULL,           // Task handle
        0               // Core (PRO_CPU)
    );

    ESP_LOGI(TAG, "ESP8266-MCP initialization complete");
}