// Updated ESP8266 TinyMCP Server with FreeRTOS Session Management
// Demonstrates the new session-based architecture with socket transport

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

// TinyMCP Session Management includes
#include "tinymcp_session.h"
#include "tinymcp_socket_transport.h"
#include "tinymcp_tools.h"

static const char *TAG = "ESP8266-MCP-Session";

// WiFi configuration - modify these for your network
#define WIFI_SSID      "FBI Surveillance Van"
#define WIFI_PASS      "jerjushanben2135"
#define WIFI_MAXIMUM_RETRY  5

// Server configuration
#define SERVER_PORT    8080
#define MAX_CONNECTIONS 3

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

// Global session manager and server
static std::unique_ptr<tinymcp::EspSocketServer> g_server;
static std::vector<std::shared_ptr<tinymcp::Session>> g_active_sessions;
static SemaphoreHandle_t g_sessions_mutex;

void print_memory_info(const char* location) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "[%s] Free heap: %u bytes, Min free: %u bytes", 
             location, (unsigned int)free_heap, (unsigned int)min_free_heap);
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

void session_cleanup_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Session cleanup task started");
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Clean up every 30 seconds
        
        if (xSemaphoreTake(g_sessions_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            auto it = g_active_sessions.begin();
            while (it != g_active_sessions.end()) {
                auto& session = *it;
                
                if (session->getState() == tinymcp::SessionState::SHUTDOWN ||
                    session->getState() == tinymcp::SessionState::ERROR_STATE) {
                    
                    ESP_LOGI(TAG, "Cleaning up finished session");
                    it = g_active_sessions.erase(it);
                } else {
                    ++it;
                }
            }
            xSemaphoreGive(g_sessions_mutex);
        }
        
        // Log session statistics
        size_t activeCount = g_active_sessions.size();
        if (activeCount > 0) {
            ESP_LOGI(TAG, "Active sessions: %zu", activeCount);
        }
        
        print_memory_info("Session cleanup");
    }
}

void session_manager_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Session manager task started");
    print_memory_info("Session manager start");
    
    // Configure socket transport
    tinymcp::SocketTransportConfig transport_config;
    transport_config.maxMessageSize = 4096;
    transport_config.receiveTimeoutMs = 5000;
    transport_config.sendTimeoutMs = 5000;
    transport_config.enableKeepAlive = true;
    
    // Create socket server
    g_server = std::make_unique<tinymcp::EspSocketServer>(SERVER_PORT, transport_config);
    
    if (g_server->start() != tinymcp::TINYMCP_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start socket server");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "TinyMCP session server listening on port %d", SERVER_PORT);
    
    while (true) {
        // Accept new connections
        auto transport = g_server->acceptConnection(1000); // 1 second timeout
        if (transport) {
            ESP_LOGI(TAG, "New client connection: %s", transport->getClientInfo().c_str());
            
            // Check connection limit
            if (g_active_sessions.size() >= MAX_CONNECTIONS) {
                ESP_LOGW(TAG, "Connection limit reached, rejecting client");
                transport->close();
                continue;
            }
            
            // Configure session
            tinymcp::SessionConfig session_config;
            session_config.maxPendingTasks = 5;
            session_config.taskStackSize = 3072; // Reduced for ESP8266
            session_config.messageQueueSize = 8;
            session_config.taskTimeoutMs = 30000;
            session_config.sessionTimeoutMs = 300000; // 5 minutes
            session_config.taskPriority = 3;
            session_config.enableProgressReporting = true;
            session_config.enableToolsPagination = false;
            
            // Create session
            auto& sessionManager = tinymcp::SessionManager::getInstance();
            auto session = sessionManager.createSession(std::move(transport), session_config);
            
            if (session) {
                // Configure server information
                session->setServerInfo("TinyMCP ESP8266 Server", "1.0.0");
                
                // Set server capabilities
                tinymcp::ServerCapabilities capabilities;
                capabilities.setProgressNotifications(true);
                capabilities.setToolsListChanged(true);
                session->setServerCapabilities(capabilities);
                
                // Add available tools from registry
                auto& toolRegistry = tinymcp::ToolRegistry::getInstance();
                auto toolNames = toolRegistry.getToolNames();
                for (const auto& toolName : toolNames) {
                    const auto* tool = toolRegistry.getTool(toolName);
                    if (tool) {
                        session->addTool(toolName, tool->description);
                    }
                }
                
                // Initialize session
                int result = session->initialize();
                if (result == tinymcp::TINYMCP_SUCCESS) {
                    // Add to active sessions
                    if (xSemaphoreTake(g_sessions_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        g_active_sessions.push_back(session);
                        xSemaphoreGive(g_sessions_mutex);
                    }
                    
                    // Start session in its own task
                    std::string taskName = "mcp_session_" + std::to_string(g_active_sessions.size());
                    
                    xTaskCreate(
                        [](void* param) {
                            auto* sessionPtr = static_cast<std::shared_ptr<tinymcp::Session>*>(param);
                            auto session = *sessionPtr;
                            delete sessionPtr;
                            
                            ESP_LOGI(TAG, "Starting session run loop");
                            session->run();
                            ESP_LOGI(TAG, "Session run loop ended");
                            
                            vTaskDelete(NULL);
                        },
                        taskName.c_str(),
                        4096, // Stack size
                        new std::shared_ptr<tinymcp::Session>(session),
                        4, // Priority
                        NULL
                    );
                    
                    ESP_LOGI(TAG, "Session created and started successfully");
                } else {
                    ESP_LOGE(TAG, "Failed to initialize session: %d", result);
                }
            } else {
                ESP_LOGE(TAG, "Failed to create session");
            }
            
            print_memory_info("After session creation");
        }
        
        // Small delay to prevent busy waiting
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

// Custom echo tool example
class EchoToolExample : public tinymcp::CallToolTask {
public:
    EchoToolExample(const tinymcp::MessageId& requestId, const std::string& toolName, const cJSON* args) :
        CallToolTask(requestId, toolName, args) {}
    
protected:
    int executeToolLogic(const cJSON* args, cJSON** result) override {
        ESP_LOGI(TAG, "Executing custom echo tool");
        
        std::string message = "Hello from custom tool!";
        if (args) {
            const cJSON* msgJson = cJSON_GetObjectItem(args, "message");
            if (msgJson && cJSON_IsString(msgJson)) {
                message = cJSON_GetStringValue(msgJson);
            }
        }
        
        cJSON* response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "echo", message.c_str());
        cJSON_AddStringToObject(response, "source", "Custom ESP8266 Tool");
        cJSON_AddNumberToObject(response, "timestamp", esp_timer_get_time() / 1000);
        
        *result = response;
        return tinymcp::TINYMCP_SUCCESS;
    }
};

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "ESP8266-MCP Session Management starting up...");
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

    // Initialize session management
    g_sessions_mutex = xSemaphoreCreateMutex();
    if (!g_sessions_mutex) {
        ESP_LOGE(TAG, "Failed to create sessions mutex");
        esp_restart();
        return;
    }

    // Register default tools
    ESP_LOGI(TAG, "Registering tools...");
    tinymcp::registerDefaultTools();
    
    // Register custom tools if needed
    auto& toolRegistry = tinymcp::ToolRegistry::getInstance();
    ESP_LOGI(TAG, "Available tools: %zu", toolRegistry.getToolNames().size());
    
    for (const auto& toolName : toolRegistry.getToolNames()) {
        ESP_LOGI(TAG, "  - %s", toolName.c_str());
    }

    ESP_LOGI(TAG, "WiFi connected, starting session manager...");

    // Create session cleanup task
    xTaskCreate(
        session_cleanup_task,
        "session_cleanup",
        2048,           // Stack size
        NULL,           // Parameters
        2,              // Priority (lower than session manager)
        NULL            // Task handle
    );

    // Create session manager task
    xTaskCreate(
        session_manager_task,
        "session_manager",
        5120,           // Stack size (larger for session management)
        NULL,           // Parameters
        5,              // Priority
        NULL            // Task handle
    );

    ESP_LOGI(TAG, "ESP8266-MCP Session Management initialization complete");
    
    // Main task can now handle other system tasks or go idle
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(60000)); // Wake up every minute
        
        // Log system health
        print_memory_info("Main task health check");
        
        // Log session manager statistics
        auto& sessionManager = tinymcp::SessionManager::getInstance();
        const auto& stats = sessionManager.getGlobalStats();
        ESP_LOGI(TAG, "Global stats - Sessions: %u, Active: %u, Messages: %u, Tasks: %u",
                stats.totalSessionsCreated, stats.activeSessions, 
                stats.totalMessages, stats.totalTasks);
    }
}