// On-Device Test Framework for FreeRTOS Session Management System
// Comprehensive testing of session lifecycle, transport, and tools
// Designed to run on ESP8266 with serial console debugging

#include <string.h>
#include <memory>
#include <vector>
#include <chrono>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"

// TinyMCP Session Management includes
#include "tinymcp_session.h"
#include "tinymcp_socket_transport.h"
#include "tinymcp_tools.h"

static const char *TAG = "SESSION_TEST";

// Test configuration
#define TEST_PORT 8080
#define TEST_TIMEOUT_MS 30000
#define MAX_TEST_SESSIONS 3

// Test result tracking
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMessage;
    uint32_t durationMs;
    
    TestResult(const std::string& name) : 
        testName(name), passed(false), durationMs(0) {}
};

class SessionTestFramework {
private:
    std::vector<TestResult> testResults;
    SemaphoreHandle_t resultsMutex;
    std::unique_ptr<tinymcp::EspSocketServer> testServer;
    
public:
    SessionTestFramework() {
        resultsMutex = xSemaphoreCreateMutex();
    }
    
    ~SessionTestFramework() {
        if (resultsMutex) {
            vSemaphoreDelete(resultsMutex);
        }
    }
    
    // Test result management
    void addResult(const TestResult& result) {
        if (xSemaphoreTake(resultsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            testResults.push_back(result);
            xSemaphoreGive(resultsMutex);
        }
    }
    
    void printResults() {
        ESP_LOGI(TAG, "\n" 
                      "╔════════════════════════════════════════════════════════════════╗\n"
                      "║                    SESSION TEST RESULTS                        ║\n"
                      "╠════════════════════════════════════════════════════════════════╣");
        
        int passed = 0, failed = 0;
        uint32_t totalTime = 0;
        
        if (xSemaphoreTake(resultsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            for (const auto& result : testResults) {
                const char* status = result.passed ? "✅ PASS" : "❌ FAIL";
                ESP_LOGI(TAG, "║ %-50s %s (%4dms) ║", 
                         result.testName.c_str(), status, result.durationMs);
                
                if (!result.passed && !result.errorMessage.empty()) {
                    ESP_LOGI(TAG, "║   Error: %-48s       ║", result.errorMessage.c_str());
                }
                
                if (result.passed) passed++;
                else failed++;
                totalTime += result.durationMs;
            }
            xSemaphoreGive(resultsMutex);
        }
        
        ESP_LOGI(TAG, "╠════════════════════════════════════════════════════════════════╣\n"
                      "║ Total: %d tests, %d passed, %d failed, %dms total time        ║\n"
                      "╚════════════════════════════════════════════════════════════════╝",
                      passed + failed, passed, failed, totalTime);
        
        // Memory info
        size_t free_heap = esp_get_free_heap_size();
        size_t min_free_heap = esp_get_minimum_free_heap_size();
        ESP_LOGI(TAG, "Memory: %u bytes free, %u bytes minimum", 
                 (unsigned int)free_heap, (unsigned int)min_free_heap);
    }
    
    // Mock transport for testing
    class MockTransport : public tinymcp::SessionTransport {
    private:
        std::vector<std::string> sendQueue;
        std::vector<std::string> receiveQueue;
        bool connected;
        SemaphoreHandle_t queueMutex;
        
    public:
        MockTransport() : connected(true) {
            queueMutex = xSemaphoreCreateMutex();
        }
        
        ~MockTransport() {
            if (queueMutex) {
                vSemaphoreDelete(queueMutex);
            }
        }
        
        int send(const std::string& data) override {
            if (!connected) return -1;
            
            if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sendQueue.push_back(data);
                ESP_LOGD(TAG, "MockTransport sent: %s", data.c_str());
                xSemaphoreGive(queueMutex);
                return data.length();
            }
            return -1;
        }
        
        int receive(std::string& data, uint32_t timeoutMs = 1000) override {
            if (!connected) return -1;
            
            TickType_t startTime = xTaskGetTickCount();
            TickType_t timeout = pdMS_TO_TICKS(timeoutMs);
            
            while ((xTaskGetTickCount() - startTime) < timeout) {
                if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    if (!receiveQueue.empty()) {
                        data = receiveQueue.front();
                        receiveQueue.erase(receiveQueue.begin());
                        ESP_LOGD(TAG, "MockTransport received: %s", data.c_str());
                        xSemaphoreGive(queueMutex);
                        return data.length();
                    }
                    xSemaphoreGive(queueMutex);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            return 0; // Timeout
        }
        
        bool isConnected() const override { return connected; }
        void close() override { connected = false; }
        std::string getClientInfo() const override { return "MockClient"; }
        
        // Test helpers
        void queueReceive(const std::string& data) {
            if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                receiveQueue.push_back(data);
                xSemaphoreGive(queueMutex);
            }
        }
        
        std::vector<std::string> getSentMessages() {
            std::vector<std::string> result;
            if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                result = sendQueue;
                xSemaphoreGive(queueMutex);
            }
            return result;
        }
        
        void disconnect() { connected = false; }
    };
    
    // Test execution helper
    TestResult runTest(const std::string& testName, std::function<bool()> testFunc) {
        TestResult result(testName);
        
        ESP_LOGI(TAG, "🧪 Running test: %s", testName.c_str());
        
        TickType_t startTime = xTaskGetTickCount();
        
        try {
            result.passed = testFunc();
        } catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
        } catch (...) {
            result.passed = false;
            result.errorMessage = "Unknown exception";
        }
        
        result.durationMs = pdTICKS_TO_MS(xTaskGetTickCount() - startTime);
        
        const char* status = result.passed ? "✅" : "❌";
        ESP_LOGI(TAG, "%s %s (%dms)", status, testName.c_str(), result.durationMs);
        
        if (!result.passed) {
            ESP_LOGE(TAG, "   Error: %s", result.errorMessage.c_str());
        }
        
        return result;
    }
    
    // Individual test methods
    bool testSessionStateTransitions() {
        auto transport = std::make_unique<MockTransport>();
        auto session = std::make_unique<tinymcp::Session>(std::move(transport));
        
        // Test initial state
        if (session->getState() != tinymcp::SessionState::UNINITIALIZED) {
            return false;
        }
        
        // Test initialization
        if (session->initialize() != 0) {
            return false;
        }
        
        // Wait for initialization to complete
        int attempts = 0;
        while (session->getState() == tinymcp::SessionState::INITIALIZING && attempts < 50) {
            vTaskDelay(pdMS_TO_TICKS(100));
            attempts++;
        }
        
        if (session->getState() != tinymcp::SessionState::INITIALIZED) {
            return false;
        }
        
        // Test shutdown
        if (session->shutdown() != 0) {
            return false;
        }
        
        return true;
    }
    
    bool testSessionConfiguration() {
        tinymcp::SessionConfig config;
        config.maxPendingTasks = 5;
        config.taskStackSize = 2048;
        config.messageQueueSize = 10;
        config.taskTimeoutMs = 15000;
        config.enableProgressReporting = true;
        
        auto transport = std::make_unique<MockTransport>();
        auto session = std::make_unique<tinymcp::Session>(std::move(transport), config);
        
        return session->initialize() == 0;
    }
    
    bool testMessageFraming() {
        std::string originalMessage = R"({"jsonrpc":"2.0","method":"test","id":1})";
        std::string framedMessage;
        std::string decodedMessage;
        
        // Test encoding
        int result = tinymcp::MessageFraming::encodeMessage(originalMessage, framedMessage);
        if (result != 0) {
            return false;
        }
        
        // Test decoding
        result = tinymcp::MessageFraming::decodeMessage(framedMessage, decodedMessage);
        if (result != 0) {
            return false;
        }
        
        return originalMessage == decodedMessage;
    }
    
    bool testToolRegistry() {
        auto& registry = tinymcp::ToolRegistry::getInstance();
        
        // Register a test tool
        auto testTool = std::make_unique<tinymcp::ToolRegistry::ToolDefinition>(
            "test_tool", 
            "A test tool",
            [](const cJSON* args, cJSON** result) -> int {
                *result = cJSON_CreateString("test_result");
                return 0;
            }
        );
        
        registry.registerTool(std::move(testTool));
        
        // Check if tool is registered
        if (!registry.hasTool("test_tool")) {
            return false;
        }
        
        // Test tool names
        auto toolNames = registry.getToolNames();
        bool foundTool = false;
        for (const auto& name : toolNames) {
            if (name == "test_tool") {
                foundTool = true;
                break;
            }
        }
        
        if (!foundTool) {
            return false;
        }
        
        // Cleanup
        registry.unregisterTool("test_tool");
        
        return !registry.hasTool("test_tool");
    }
    
    bool testAsyncTaskExecution() {
        class TestTask : public tinymcp::AsyncTask {
        public:
            TestTask() : AsyncTask(tinymcp::MessageId("test"), "test_method") {}
            
            bool isValid() const override { return true; }
            
            int execute() override {
                // Simulate some work with progress reporting
                for (int i = 0; i <= 10; i++) {
                    if (isCancelled()) {
                        return TINYMCP_ERROR_CANCELLED;
                    }
                    
                    reportProgress(i, 10, "Processing step " + std::to_string(i));
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                return 0;
            }
        };
        
        auto task = std::make_unique<TestTask>();
        TestTask* taskPtr = task.get();
        
        // Execute task
        int result = task->execute();
        
        return result == 0 && taskPtr->isFinished();
    }
    
    bool testSessionManager() {
        auto& manager = tinymcp::SessionManager::getInstance();
        
        // Create test sessions
        std::vector<std::shared_ptr<tinymcp::Session>> sessions;
        
        for (int i = 0; i < 3; i++) {
            auto transport = std::make_unique<MockTransport>();
            auto session = manager.createSession(std::move(transport));
            if (!session) {
                return false;
            }
            sessions.push_back(session);
        }
        
        // Check session count
        if (manager.getSessionCount() != 3) {
            return false;
        }
        
        // Remove sessions
        for (auto& session : sessions) {
            manager.removeSession(session);
        }
        
        // Check cleanup
        manager.cleanupInactiveSessions();
        
        return true;
    }
    
    bool testSocketTransportConfig() {
        tinymcp::SocketTransportConfig config;
        config.receiveTimeoutMs = 5000;
        config.sendTimeoutMs = 3000;
        config.maxMessageSize = 4096;
        config.enableKeepAlive = true;
        
        // Test with invalid socket (should handle gracefully)
        auto transport = std::make_unique<tinymcp::EspSocketTransport>(-1, config);
        
        // Should not be connected with invalid socket
        return !transport->isConnected();
    }
    
    bool testMemoryUsage() {
        size_t initialHeap = esp_get_free_heap_size();
        
        // Create and destroy multiple sessions to test for memory leaks
        for (int iteration = 0; iteration < 5; iteration++) {
            std::vector<std::shared_ptr<tinymcp::Session>> sessions;
            
            for (int i = 0; i < 3; i++) {
                auto transport = std::make_unique<MockTransport>();
                auto session = std::make_unique<tinymcp::Session>(std::move(transport));
                
                session->initialize();
                
                // Create some async tasks
                for (int j = 0; j < 2; j++) {
                    class QuickTask : public tinymcp::AsyncTask {
                    public:
                        QuickTask(int id) : AsyncTask(tinymcp::MessageId("task_" + std::to_string(id)), "quick") {}
                        bool isValid() const override { return true; }
                        int execute() override { vTaskDelay(pdMS_TO_TICKS(10)); return 0; }
                    };
                    
                    auto task = std::make_unique<QuickTask>(j);
                    session->submitTask(std::move(task));
                }
                
                sessions.push_back(std::move(session));
            }
            
            // Wait for tasks to complete
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Cleanup sessions
            for (auto& session : sessions) {
                session->shutdown();
            }
            sessions.clear();
            
            // Force garbage collection
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        size_t finalHeap = esp_get_free_heap_size();
        
        // Allow for some memory fragmentation, but no major leaks
        int heapDiff = (int)initialHeap - (int)finalHeap;
        ESP_LOGI(TAG, "Memory usage test: Initial=%u, Final=%u, Diff=%d", 
                 (unsigned)initialHeap, (unsigned)finalHeap, heapDiff);
        
        return abs(heapDiff) < 1024; // Less than 1KB difference acceptable
    }
    
    bool testConcurrentSessions() {
        auto& manager = tinymcp::SessionManager::getInstance();
        std::vector<std::shared_ptr<tinymcp::Session>> sessions;
        
        // Create multiple concurrent sessions
        for (int i = 0; i < MAX_TEST_SESSIONS; i++) {
            auto transport = std::make_unique<MockTransport>();
            auto session = manager.createSession(std::move(transport));
            
            if (!session || session->initialize() != 0) {
                return false;
            }
            
            sessions.push_back(session);
        }
        
        // All sessions should be running
        bool allActive = true;
        for (auto& session : sessions) {
            if (session->getState() != tinymcp::SessionState::INITIALIZED && 
                session->getState() != tinymcp::SessionState::ACTIVE) {
                allActive = false;
                break;
            }
        }
        
        // Cleanup
        for (auto& session : sessions) {
            session->shutdown();
            manager.removeSession(session);
        }
        
        return allActive;
    }
    
    // Main test runner
    void runAllTests() {
        ESP_LOGI(TAG, "\n"
                      "╔════════════════════════════════════════════════════════════════╗\n"
                      "║              FREERTOS SESSION MANAGEMENT TESTS                ║\n"
                      "║                    Starting Test Suite                         ║\n"
                      "╚════════════════════════════════════════════════════════════════╝");
        
        // Register default tools for testing
        tinymcp::registerDefaultTools();
        
        // Run all tests
        addResult(runTest("Session State Transitions", [this]() { return testSessionStateTransitions(); }));
        addResult(runTest("Session Configuration", [this]() { return testSessionConfiguration(); }));
        addResult(runTest("Message Framing", [this]() { return testMessageFraming(); }));
        addResult(runTest("Tool Registry", [this]() { return testToolRegistry(); }));
        addResult(runTest("Async Task Execution", [this]() { return testAsyncTaskExecution(); }));
        addResult(runTest("Session Manager", [this]() { return testSessionManager(); }));
        addResult(runTest("Socket Transport Config", [this]() { return testSocketTransportConfig(); }));
        addResult(runTest("Memory Usage", [this]() { return testMemoryUsage(); }));
        addResult(runTest("Concurrent Sessions", [this]() { return testConcurrentSessions(); }));
        
        // Print final results
        printResults();
        
        ESP_LOGI(TAG, "\n🏁 Test suite completed! Check results above.");
    }
};

// Test task function
static void sessionTestTask(void* pvParameters) {
    ESP_LOGI(TAG, "🚀 Starting FreeRTOS Session Management System Tests");
    
    // Wait for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    SessionTestFramework testFramework;
    testFramework.runAllTests();
    
    ESP_LOGI(TAG, "✨ Session tests completed. Task will now terminate.");
    vTaskDelete(NULL);
}

// Public interface to start tests
extern "C" void start_session_tests() {
    ESP_LOGI(TAG, "🔧 Initializing Session Test Framework...");
    
    // Create test task
    TaskHandle_t testTaskHandle = NULL;
    BaseType_t result = xTaskCreate(
        sessionTestTask,
        "session_test",
        8192,  // Stack size - larger for comprehensive tests
        NULL,
        5,     // Priority
        &testTaskHandle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "❌ Failed to create session test task!");
        return;
    }
    
    ESP_LOGI(TAG, "✅ Session test task created successfully");
}

// Integration with main application
extern "C" void app_main() {
    ESP_LOGI(TAG, "\n"
                  "╔════════════════════════════════════════════════════════════════╗\n"
                  "║                 ESP8266 SESSION TEST SUITE                    ║\n"
                  "║             FreeRTOS Session Management Testing               ║\n"
                  "╚════════════════════════════════════════════════════════════════╝");
    
    // Start the test suite
    start_session_tests();
    
    // Keep main task alive
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // Print memory status every 10 seconds
        size_t free_heap = esp_get_free_heap_size();
        ESP_LOGI(TAG, "💾 Free heap: %u bytes", (unsigned int)free_heap);
    }
}