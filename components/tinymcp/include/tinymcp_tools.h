#pragma once

// Example Tool Implementations for TinyMCP FreeRTOS Session Management
// Provides concrete tool examples for ESP8266/ESP32 platforms

#include "tinymcp_session.h"
#include <memory>
#include <string>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

namespace tinymcp {

// Tool registry for managing available tools
class ToolRegistry {
public:
    // Tool definition structure
    struct ToolDefinition {
        std::string name;
        std::string description;
        cJSON* inputSchema;
        std::function<int(const cJSON*, cJSON**)> handler;
        bool requiresAsync;
        uint32_t estimatedDurationMs;
        
        ToolDefinition(const std::string& n, const std::string& d, 
                      std::function<int(const cJSON*, cJSON**)> h,
                      bool async = false, uint32_t duration = 1000) :
            name(n), description(d), inputSchema(nullptr), handler(h),
            requiresAsync(async), estimatedDurationMs(duration) {}
        
        ~ToolDefinition() {
            if (inputSchema) {
                cJSON_Delete(inputSchema);
            }
        }
    };
    
    static ToolRegistry& getInstance();
    
    // Tool management
    void registerTool(std::unique_ptr<ToolDefinition> tool);
    void unregisterTool(const std::string& name);
    bool hasTool(const std::string& name) const;
    const ToolDefinition* getTool(const std::string& name) const;
    std::vector<std::string> getToolNames() const;
    
    // Tool execution
    std::unique_ptr<AsyncTask> createToolTask(const MessageId& requestId, 
                                             const std::string& toolName,
                                             const cJSON* arguments);

private:
    ToolRegistry() = default;
    static ToolRegistry instance_;
    std::unordered_map<std::string, std::unique_ptr<ToolDefinition>> tools_;
    SemaphoreHandle_t registryMutex_;
};

// Base class for custom tool tasks
class CustomToolTask : public CallToolTask {
public:
    CustomToolTask(const MessageId& requestId, const std::string& toolName, 
                   const cJSON* args, std::function<int(const cJSON*, cJSON**)> handler);
    
protected:
    int executeToolLogic(const cJSON* args, cJSON** result) override;
    
private:
    std::function<int(const cJSON*, cJSON**)> toolHandler_;
};

// Example tool implementations

// System Information Tool
class SystemInfoTool {
public:
    static void registerTool();
    static int execute(const cJSON* args, cJSON** result);
    
private:
    static cJSON* createInputSchema();
    static cJSON* getSystemInfo();
    static cJSON* getMemoryInfo();
    static cJSON* getWiFiInfo();
    static cJSON* getTaskInfo();
};

// GPIO Control Tool
class GPIOControlTool {
public:
    static void registerTool();
    static int execute(const cJSON* args, cJSON** result);
    
private:
    static cJSON* createInputSchema();
    static int setGPIOPin(int pin, bool state);
    static int getGPIOPin(int pin, bool* state);
    static bool isValidGPIOPin(int pin);
};

// Network Scanner Tool (async example)
class NetworkScannerTask : public AsyncTask {
public:
    NetworkScannerTask(const MessageId& requestId, const cJSON* args);
    
    bool isValid() const override;
    int execute() override;
    
    static void registerTool();
    static std::unique_ptr<AsyncTask> create(const MessageId& requestId, const cJSON* args);
    
private:
    struct ScanParams {
        bool includeBSSID;
        bool includeRSSI;
        bool includeChannel;
        int maxResults;
        uint32_t timeoutMs;
        
        ScanParams() : includeBSSID(true), includeRSSI(true), 
                      includeChannel(true), maxResults(20), timeoutMs(10000) {}
    };
    
    ScanParams params_;
    cJSON* scanResults_;
    
    static cJSON* createInputSchema();
    int performWiFiScan();
    cJSON* formatScanResults();
    void parseArguments(const cJSON* args);
};

// File System Tool
class FileSystemTool {
public:
    static void registerTool();
    static int execute(const cJSON* args, cJSON** result);
    
private:
    enum class Operation {
        LIST_FILES,
        READ_FILE,
        WRITE_FILE,
        DELETE_FILE,
        GET_INFO
    };
    
    static cJSON* createInputSchema();
    static Operation parseOperation(const cJSON* args);
    static int listFiles(const std::string& path, cJSON** result);
    static int readFile(const std::string& filename, cJSON** result);
    static int writeFile(const std::string& filename, const std::string& content, cJSON** result);
    static int deleteFile(const std::string& filename, cJSON** result);
    static int getFileInfo(const std::string& filename, cJSON** result);
    static bool isValidPath(const std::string& path);
};

// Echo/Test Tool (simple synchronous example)
class EchoTool {
public:
    static void registerTool();
    static int execute(const cJSON* args, cJSON** result);
    
private:
    static cJSON* createInputSchema();
};

// Long Running Task Example (demonstrates progress reporting)
class LongRunningTask : public AsyncTask {
public:
    LongRunningTask(const MessageId& requestId, const cJSON* args);
    
    bool isValid() const override;
    int execute() override;
    
    static void registerTool();
    static std::unique_ptr<AsyncTask> create(const MessageId& requestId, const cJSON* args);
    
private:
    struct TaskParams {
        uint32_t durationSeconds;
        uint32_t stepCount;
        bool simulateError;
        std::string message;
        
        TaskParams() : durationSeconds(10), stepCount(10), simulateError(false) {}
    };
    
    TaskParams params_;
    
    static cJSON* createInputSchema();
    void parseArguments(const cJSON* args);
    int performLongRunningWork();
};

// I2C Scanner Tool (hardware interaction example)
class I2CScannerTool {
public:
    static void registerTool();
    static int execute(const cJSON* args, cJSON** result);
    
private:
    static cJSON* createInputSchema();
    static int scanI2CBus(int sda, int scl, cJSON** result);
    static bool isValidI2CPin(int pin);
};

// Tool registration helper functions
namespace ToolHelpers {
    // Schema builders
    cJSON* createStringProperty(const std::string& description, bool required = false);
    cJSON* createIntegerProperty(const std::string& description, int min = INT_MIN, int max = INT_MAX, bool required = false);
    cJSON* createBooleanProperty(const std::string& description, bool required = false);
    cJSON* createEnumProperty(const std::string& description, const std::vector<std::string>& values, bool required = false);
    cJSON* createObjectSchema(const std::vector<std::pair<std::string, cJSON*>>& properties);
    
    // Validation helpers
    bool validateStringParam(const cJSON* params, const std::string& name, std::string& value, bool required = true);
    bool validateIntParam(const cJSON* params, const std::string& name, int& value, bool required = true);
    bool validateBoolParam(const cJSON* params, const std::string& name, bool& value, bool required = true);
    
    // Response builders
    cJSON* createSuccessResponse(const cJSON* data = nullptr);
    cJSON* createErrorResponse(const std::string& error, int code = -1);
    cJSON* createProgressResponse(int current, int total, const std::string& message = "");
    
    // Utility functions
    std::string formatBytes(size_t bytes);
    std::string formatDuration(uint32_t milliseconds);
    bool isValidFileName(const std::string& filename);
    std::string sanitizePath(const std::string& path);
}

// Auto-registration helper macro
#define REGISTER_TOOL_ON_STARTUP(ToolClass) \
    static void __attribute__((constructor)) register_##ToolClass() { \
        ToolClass::registerTool(); \
    }

// Default tool registration function
void registerDefaultTools();

} // namespace tinymcp