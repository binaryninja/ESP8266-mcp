// Example Tool Implementations for TinyMCP FreeRTOS Session Management
// Provides concrete tool examples for ESP8266/ESP32 platforms

#include "tinymcp_tools.h"
#include "tinymcp_constants.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>
#include <algorithm>
#include <fstream>
#include <dirent.h>

static const char* TAG = "tinymcp_tools";

namespace tinymcp {

// ToolRegistry singleton
ToolRegistry ToolRegistry::instance_;

ToolRegistry& ToolRegistry::getInstance() {
    return instance_;
}

void ToolRegistry::registerTool(std::unique_ptr<ToolDefinition> tool) {
    if (!registryMutex_) {
        registryMutex_ = xSemaphoreCreateMutex();
    }
    
    if (xSemaphoreTake(registryMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        tools_[tool->name] = std::move(tool);
        xSemaphoreGive(registryMutex_);
        ESP_LOGI(TAG, "Registered tool: %s", tool->name.c_str());
    }
}

void ToolRegistry::unregisterTool(const std::string& name) {
    if (registryMutex_ && xSemaphoreTake(registryMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            tools_.erase(it);
            ESP_LOGI(TAG, "Unregistered tool: %s", name.c_str());
        }
        xSemaphoreGive(registryMutex_);
    }
}

bool ToolRegistry::hasTool(const std::string& name) const {
    if (!registryMutex_) return false;
    
    bool found = false;
    if (xSemaphoreTake(registryMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        found = tools_.find(name) != tools_.end();
        xSemaphoreGive(registryMutex_);
    }
    return found;
}

const ToolRegistry::ToolDefinition* ToolRegistry::getTool(const std::string& name) const {
    if (!registryMutex_) return nullptr;
    
    const ToolDefinition* tool = nullptr;
    if (xSemaphoreTake(registryMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            tool = it->second.get();
        }
        xSemaphoreGive(registryMutex_);
    }
    return tool;
}

std::vector<std::string> ToolRegistry::getToolNames() const {
    std::vector<std::string> names;
    if (!registryMutex_) return names;
    
    if (xSemaphoreTake(registryMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (const auto& [name, tool] : tools_) {
            names.push_back(name);
        }
        xSemaphoreGive(registryMutex_);
    }
    return names;
}

std::unique_ptr<AsyncTask> ToolRegistry::createToolTask(const MessageId& requestId, 
                                                       const std::string& toolName,
                                                       const cJSON* arguments) {
    const ToolDefinition* tool = getTool(toolName);
    if (!tool) {
        return std::make_unique<ErrorTask>(requestId, TINYMCP_ERROR_TOOL_NOT_FOUND, 
                                          "Tool not found: " + toolName);
    }
    
    if (tool->requiresAsync) {
        // Special handling for async tools
        if (toolName == "network_scan") {
            return NetworkScannerTask::create(requestId, arguments);
        } else if (toolName == "long_running_task") {
            return LongRunningTask::create(requestId, arguments);
        }
    }
    
    // Create a synchronous tool task
    return std::make_unique<CustomToolTask>(requestId, toolName, arguments, tool->handler);
}

// CustomToolTask implementation
CustomToolTask::CustomToolTask(const MessageId& requestId, const std::string& toolName, 
                              const cJSON* args, std::function<int(const cJSON*, cJSON**)> handler) :
    CallToolTask(requestId, toolName, args), toolHandler_(handler) {
}

int CustomToolTask::executeToolLogic(const cJSON* args, cJSON** result) {
    if (!toolHandler_) {
        return TINYMCP_ERROR_TOOL_NOT_FOUND;
    }
    
    return toolHandler_(args, result);
}

// SystemInfoTool implementation
void SystemInfoTool::registerTool() {
    auto tool = std::make_unique<ToolRegistry::ToolDefinition>(
        "system_info",
        "Get ESP8266/ESP32 system information including memory, WiFi status, and running tasks",
        [](const cJSON* args, cJSON** result) { return SystemInfoTool::execute(args, result); }
    );
    
    tool->inputSchema = createInputSchema();
    ToolRegistry::getInstance().registerTool(std::move(tool));
}

int SystemInfoTool::execute(const cJSON* args, cJSON** result) {
    ESP_LOGI(TAG, "Executing system_info tool");
    
    cJSON* response = cJSON_CreateObject();
    if (!response) {
        return TINYMCP_ERROR_OUT_OF_MEMORY;
    }
    
    // Add system information sections
    cJSON_AddItemToObject(response, "system", getSystemInfo());
    cJSON_AddItemToObject(response, "memory", getMemoryInfo());
    cJSON_AddItemToObject(response, "wifi", getWiFiInfo());
    cJSON_AddItemToObject(response, "tasks", getTaskInfo());
    
    *result = response;
    return TINYMCP_SUCCESS;
}

cJSON* SystemInfoTool::createInputSchema() {
    return ToolHelpers::createObjectSchema({
        {"include_tasks", ToolHelpers::createBooleanProperty("Include FreeRTOS task information", false)},
        {"include_wifi", ToolHelpers::createBooleanProperty("Include WiFi status information", false)}
    });
}

cJSON* SystemInfoTool::getSystemInfo() {
    cJSON* system = cJSON_CreateObject();
    
    // Chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    cJSON_AddStringToObject(system, "chip_model", "ESP8266");
    cJSON_AddNumberToObject(system, "chip_revision", chip_info.revision);
    cJSON_AddNumberToObject(system, "cpu_cores", chip_info.cores);
    cJSON_AddNumberToObject(system, "cpu_freq_mhz", 80); // ESP8266 default 80MHz
    
    // Flash information
    cJSON_AddNumberToObject(system, "flash_size", 4 * 1024 * 1024); // 4MB default for ESP8266
    
    // SDK version
    cJSON_AddStringToObject(system, "idf_version", esp_get_idf_version());
    
    // Uptime
    cJSON_AddNumberToObject(system, "uptime_ms", xTaskGetTickCount() * portTICK_PERIOD_MS);
    
    return system;
}

cJSON* SystemInfoTool::getMemoryInfo() {
    cJSON* memory = cJSON_CreateObject();
    
    // Heap information
    cJSON_AddNumberToObject(memory, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(memory, "minimum_free_heap", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(memory, "largest_free_block", esp_get_free_heap_size());
    
    // Internal memory (ESP8266 doesn't have heap_caps, use free heap)
    cJSON_AddNumberToObject(memory, "free_internal", esp_get_free_heap_size());
    
    return memory;
}

cJSON* SystemInfoTool::getWiFiInfo() {
    cJSON* wifi = cJSON_CreateObject();
    
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        const char* modeStr = "UNKNOWN";
        switch (mode) {
            case WIFI_MODE_STA: modeStr = "STA"; break;
            case WIFI_MODE_AP: modeStr = "AP"; break;
            case WIFI_MODE_APSTA: modeStr = "AP+STA"; break;
            default: break;
        }
        cJSON_AddStringToObject(wifi, "mode", modeStr);
        
        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                cJSON_AddStringToObject(wifi, "ssid", (char*)ap_info.ssid);
                cJSON_AddNumberToObject(wifi, "rssi", ap_info.rssi);
                cJSON_AddNumberToObject(wifi, "channel", ap_info.primary);
            }
        }
    }
    
    return wifi;
}

cJSON* SystemInfoTool::getTaskInfo() {
    cJSON* tasks = cJSON_CreateArray();
    
    // Get task count
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    cJSON_AddNumberToObject(tasks, "task_count", taskCount);
    
    // Note: Getting detailed task information requires more complex FreeRTOS APIs
    // that may not be available in all configurations
    
    return tasks;
}

// GPIOControlTool implementation
void GPIOControlTool::registerTool() {
    auto tool = std::make_unique<ToolRegistry::ToolDefinition>(
        "gpio_control",
        "Control ESP8266/ESP32 GPIO pins - set output state or read input state",
        [](const cJSON* args, cJSON** result) { return GPIOControlTool::execute(args, result); }
    );
    
    tool->inputSchema = createInputSchema();
    ToolRegistry::getInstance().registerTool(std::move(tool));
}

int GPIOControlTool::execute(const cJSON* args, cJSON** result) {
    if (!args) {
        return TINYMCP_ERROR_INVALID_PARAMS;
    }
    
    std::string operation;
    int pin;
    
    if (!ToolHelpers::validateStringParam(args, "operation", operation) ||
        !ToolHelpers::validateIntParam(args, "pin", pin)) {
        return TINYMCP_ERROR_INVALID_PARAMS;
    }
    
    if (!isValidGPIOPin(pin)) {
        *result = ToolHelpers::createErrorResponse("Invalid GPIO pin: " + std::to_string(pin));
        return TINYMCP_SUCCESS;
    }
    
    cJSON* response = cJSON_CreateObject();
    
    if (operation == "set") {
        bool state;
        if (!ToolHelpers::validateBoolParam(args, "state", state)) {
            cJSON_Delete(response);
            return TINYMCP_ERROR_INVALID_PARAMS;
        }
        
        int result_code = setGPIOPin(pin, state);
        if (result_code == TINYMCP_SUCCESS) {
            cJSON_AddStringToObject(response, "status", "success");
            cJSON_AddNumberToObject(response, "pin", pin);
            cJSON_AddBoolToObject(response, "state", state);
        } else {
            cJSON_Delete(response);
            *result = ToolHelpers::createErrorResponse("Failed to set GPIO pin");
            return TINYMCP_SUCCESS;
        }
    } else if (operation == "get") {
        bool state;
        int result_code = getGPIOPin(pin, &state);
        if (result_code == TINYMCP_SUCCESS) {
            cJSON_AddStringToObject(response, "status", "success");
            cJSON_AddNumberToObject(response, "pin", pin);
            cJSON_AddBoolToObject(response, "state", state);
        } else {
            cJSON_Delete(response);
            *result = ToolHelpers::createErrorResponse("Failed to read GPIO pin");
            return TINYMCP_SUCCESS;
        }
    } else {
        cJSON_Delete(response);
        *result = ToolHelpers::createErrorResponse("Invalid operation: " + operation);
        return TINYMCP_SUCCESS;
    }
    
    *result = response;
    return TINYMCP_SUCCESS;
}

cJSON* GPIOControlTool::createInputSchema() {
    return ToolHelpers::createObjectSchema({
        {"operation", ToolHelpers::createEnumProperty("GPIO operation", {"set", "get"}, true)},
        {"pin", ToolHelpers::createIntegerProperty("GPIO pin number", 0, 16, true)},
        {"state", ToolHelpers::createBooleanProperty("Pin state (for set operation)", false)}
    });
}

int GPIOControlTool::setGPIOPin(int pin, bool state) {
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << pin);
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    
    if (gpio_config(&config) != ESP_OK) {
        return TINYMCP_ERROR_HARDWARE_FAILED;
    }
    
    if (gpio_set_level((gpio_num_t)pin, state ? 1 : 0) != ESP_OK) {
        return TINYMCP_ERROR_HARDWARE_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

int GPIOControlTool::getGPIOPin(int pin, bool* state) {
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << pin);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    
    if (gpio_config(&config) != ESP_OK) {
        return TINYMCP_ERROR_HARDWARE_FAILED;
    }
    
    int level = gpio_get_level((gpio_num_t)pin);
    *state = (level == 1);
    
    return TINYMCP_SUCCESS;
}

bool GPIOControlTool::isValidGPIOPin(int pin) {
    // ESP8266 has GPIO pins 0-16, but some are restricted
    return (pin >= 0 && pin <= 16 && pin != 6 && pin != 7 && pin != 8 && pin != 11);
}

// EchoTool implementation
void EchoTool::registerTool() {
    auto tool = std::make_unique<ToolRegistry::ToolDefinition>(
        "echo",
        "Simple echo tool that returns the input message for testing",
        [](const cJSON* args, cJSON** result) { return EchoTool::execute(args, result); }
    );
    
    tool->inputSchema = createInputSchema();
    ToolRegistry::getInstance().registerTool(std::move(tool));
}

int EchoTool::execute(const cJSON* args, cJSON** result) {
    std::string message = "Hello from TinyMCP!";
    
    if (args) {
        ToolHelpers::validateStringParam(args, "message", message, false);
    }
    
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "echo", message.c_str());
    cJSON_AddNumberToObject(response, "timestamp", xTaskGetTickCount() * portTICK_PERIOD_MS);
    cJSON_AddStringToObject(response, "source", "ESP8266 TinyMCP Server");
    
    *result = response;
    return TINYMCP_SUCCESS;
}

cJSON* EchoTool::createInputSchema() {
    return ToolHelpers::createObjectSchema({
        {"message", ToolHelpers::createStringProperty("Message to echo back", false)}
    });
}

// NetworkScannerTask implementation
NetworkScannerTask::NetworkScannerTask(const MessageId& requestId, const cJSON* args) :
    AsyncTask(requestId, "network_scan"), scanResults_(nullptr) {
    
    parseArguments(args);
    setTimeout(params_.timeoutMs);
}

bool NetworkScannerTask::isValid() const {
    return params_.maxResults > 0 && params_.timeoutMs > 0;
}

std::unique_ptr<AsyncTask> NetworkScannerTask::create(const MessageId& requestId, const cJSON* args) {
    return std::make_unique<NetworkScannerTask>(requestId, args);
}

void NetworkScannerTask::registerTool() {
    auto tool = std::make_unique<ToolRegistry::ToolDefinition>(
        "network_scan",
        "Scan for available WiFi networks (async operation with progress reporting)",
        nullptr, // No synchronous handler
        true,    // Requires async
        10000    // Estimated 10 seconds
    );
    
    tool->inputSchema = createInputSchema();
    ToolRegistry::getInstance().registerTool(std::move(tool));
}

int NetworkScannerTask::execute() {
    if (cancelled_ || finished_) {
        return TINYMCP_ERROR_CANCELLED;
    }
    
    ESP_LOGI(TAG, "Starting WiFi network scan");
    
    reportProgress(0, 100, "Starting WiFi scan...");
    
    int result = performWiFiScan();
    
    if (result == TINYMCP_SUCCESS) {
        reportProgress(100, 100, "WiFi scan completed");
        finished_ = true;
    } else {
        finished_ = true;
        ESP_LOGE(TAG, "WiFi scan failed: %d", result);
    }
    
    return result;
}

cJSON* NetworkScannerTask::createInputSchema() {
    return ToolHelpers::createObjectSchema({
        {"include_bssid", ToolHelpers::createBooleanProperty("Include BSSID in results", false)},
        {"include_rssi", ToolHelpers::createBooleanProperty("Include signal strength", false)},
        {"include_channel", ToolHelpers::createBooleanProperty("Include channel information", false)},
        {"max_results", ToolHelpers::createIntegerProperty("Maximum number of results", 1, 50, false)},
        {"timeout_ms", ToolHelpers::createIntegerProperty("Scan timeout in milliseconds", 1000, 30000, false)}
    });
}

int NetworkScannerTask::performWiFiScan() {
    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 100;
    scan_config.scan_time.active.max = 300;
    
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        return TINYMCP_ERROR_HARDWARE_FAILED;
    }
    
    reportProgress(50, 100, "Scanning networks...");
    
    uint16_t number = params_.maxResults;
    wifi_ap_record_t* ap_info = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * number);
    if (!ap_info) {
        return TINYMCP_ERROR_OUT_OF_MEMORY;
    }
    
    if (esp_wifi_scan_get_ap_records(&number, ap_info) != ESP_OK) {
        free(ap_info);
        return TINYMCP_ERROR_HARDWARE_FAILED;
    }
    
    // Format results
    scanResults_ = cJSON_CreateArray();
    for (int i = 0; i < number; i++) {
        cJSON* network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", (char*)ap_info[i].ssid);
        
        if (params_.includeRSSI) {
            cJSON_AddNumberToObject(network, "rssi", ap_info[i].rssi);
        }
        
        if (params_.includeChannel) {
            cJSON_AddNumberToObject(network, "channel", ap_info[i].primary);
        }
        
        if (params_.includeBSSID) {
            char bssid_str[18];
            snprintf(bssid_str, sizeof(bssid_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                    ap_info[i].bssid[0], ap_info[i].bssid[1], ap_info[i].bssid[2],
                    ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);
            cJSON_AddStringToObject(network, "bssid", bssid_str);
        }
        
        cJSON_AddItemToArray(scanResults_, network);
    }
    
    free(ap_info);
    return TINYMCP_SUCCESS;
}

void NetworkScannerTask::parseArguments(const cJSON* args) {
    if (!args) return;
    
    ToolHelpers::validateBoolParam(args, "include_bssid", params_.includeBSSID, false);
    ToolHelpers::validateBoolParam(args, "include_rssi", params_.includeRSSI, false);
    ToolHelpers::validateBoolParam(args, "include_channel", params_.includeChannel, false);
    ToolHelpers::validateIntParam(args, "max_results", params_.maxResults, false);
    ToolHelpers::validateIntParam(args, "timeout_ms", (int&)params_.timeoutMs, false);
}

// ToolHelpers implementation
namespace ToolHelpers {

cJSON* createStringProperty(const std::string& description, bool required) {
    cJSON* prop = cJSON_CreateObject();
    cJSON_AddStringToObject(prop, "type", "string");
    cJSON_AddStringToObject(prop, "description", description.c_str());
    if (required) {
        cJSON_AddBoolToObject(prop, "required", true);
    }
    return prop;
}

cJSON* createIntegerProperty(const std::string& description, int min, int max, bool required) {
    cJSON* prop = cJSON_CreateObject();
    cJSON_AddStringToObject(prop, "type", "integer");
    cJSON_AddStringToObject(prop, "description", description.c_str());
    if (min != INT_MIN) cJSON_AddNumberToObject(prop, "minimum", min);
    if (max != INT_MAX) cJSON_AddNumberToObject(prop, "maximum", max);
    if (required) {
        cJSON_AddBoolToObject(prop, "required", true);
    }
    return prop;
}

cJSON* createBooleanProperty(const std::string& description, bool required) {
    cJSON* prop = cJSON_CreateObject();
    cJSON_AddStringToObject(prop, "type", "boolean");
    cJSON_AddStringToObject(prop, "description", description.c_str());
    if (required) {
        cJSON_AddBoolToObject(prop, "required", true);
    }
    return prop;
}

cJSON* createEnumProperty(const std::string& description, const std::vector<std::string>& values, bool required) {
    cJSON* prop = cJSON_CreateObject();
    cJSON_AddStringToObject(prop, "type", "string");
    cJSON_AddStringToObject(prop, "description", description.c_str());
    
    cJSON* enumArray = cJSON_CreateArray();
    for (const auto& value : values) {
        cJSON_AddItemToArray(enumArray, cJSON_CreateString(value.c_str()));
    }
    cJSON_AddItemToObject(prop, "enum", enumArray);
    
    if (required) {
        cJSON_AddBoolToObject(prop, "required", true);
    }
    return prop;
}

cJSON* createObjectSchema(const std::vector<std::pair<std::string, cJSON*>>& properties) {
    cJSON* schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "object");
    
    cJSON* props = cJSON_CreateObject();
    for (const auto& [name, prop] : properties) {
        cJSON_AddItemToObject(props, name.c_str(), prop);
    }
    cJSON_AddItemToObject(schema, "properties", props);
    
    return schema;
}

bool validateStringParam(const cJSON* params, const std::string& name, std::string& value, bool required) {
    const cJSON* param = cJSON_GetObjectItem(params, name.c_str());
    if (!param) {
        return !required;
    }
    
    if (!cJSON_IsString(param)) {
        return false;
    }
    
    value = cJSON_GetStringValue(const_cast<cJSON*>(param));
    return true;
}

bool validateIntParam(const cJSON* params, const std::string& name, int& value, bool required) {
    const cJSON* param = cJSON_GetObjectItem(params, name.c_str());
    if (!param) {
        return !required;
    }
    
    if (!cJSON_IsNumber(param)) {
        return false;
    }
    
    value = param->valuedouble;
    return true;
}

bool validateBoolParam(const cJSON* params, const std::string& name, bool& value, bool required) {
    const cJSON* param = cJSON_GetObjectItem(params, name.c_str());
    if (!param) {
        return !required;
    }
    
    if (!cJSON_IsBool(param)) {
        return false;
    }
    
    value = cJSON_IsTrue(param);
    return true;
}

cJSON* createSuccessResponse(const cJSON* data) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "success");
    if (data) {
        cJSON_AddItemToObject(response, "data", cJSON_Duplicate(data, cJSON_True));
    }
    return response;
}

cJSON* createErrorResponse(const std::string& error, int code) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "error");
    cJSON_AddStringToObject(response, "message", error.c_str());
    if (code != -1) {
        cJSON_AddNumberToObject(response, "code", code);
    }
    return response;
}

std::string formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unit]);
    return std::string(buffer);
}

std::string formatDuration(uint32_t milliseconds) {
    if (milliseconds < 1000) {
        return std::to_string(milliseconds) + "ms";
    } else if (milliseconds < 60000) {
        return std::to_string(milliseconds / 1000) + "s";
    } else {
        return std::to_string(milliseconds / 60000) + "m " + 
               std::to_string((milliseconds % 60000) / 1000) + "s";
    }
}

} // namespace ToolHelpers

// Default tool registration
void registerDefaultTools() {
    ESP_LOGI(TAG, "Registering default tools...");
    
    SystemInfoTool::registerTool();
    GPIOControlTool::registerTool();
    EchoTool::registerTool();
    NetworkScannerTask::registerTool();
    
    ESP_LOGI(TAG, "Default tools registered");
}

} // namespace tinymcp