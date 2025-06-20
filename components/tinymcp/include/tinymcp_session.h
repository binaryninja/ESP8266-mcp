#pragma once

// FreeRTOS Session Management for TinyMCP Protocol
// ESP8266/ESP32 optimized session handling with async task support

#include <string>
#include <memory>
#include <unordered_map>
#include <deque>
#include <vector>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "tinymcp_constants.h"
#include "tinymcp_message.h"
#include "tinymcp_request.h"
#include "tinymcp_response.h"
#include "tinymcp_notification.h"

namespace tinymcp {

// Forward declarations
class SessionTransport;
class AsyncTask;
class SessionManager;

// Session state enumeration
enum class SessionState : uint8_t {
    UNINITIALIZED = 0,
    INITIALIZING,
    INITIALIZED,
    ACTIVE,
    SHUTTING_DOWN,
    SHUTDOWN,
    ERROR_STATE
};

// Session configuration
struct SessionConfig {
    uint32_t maxPendingTasks;       // Maximum pending async tasks
    uint32_t taskStackSize;         // Stack size for async tasks
    uint32_t messageQueueSize;      // Message queue depth
    uint32_t taskTimeoutMs;         // Task timeout in milliseconds
    uint32_t sessionTimeoutMs;      // Session timeout in milliseconds
    uint8_t taskPriority;           // Priority for async tasks
    bool enableProgressReporting;   // Enable progress notifications
    bool enableToolsPagination;     // Enable tools pagination
    
    SessionConfig() :
        maxPendingTasks(8),
        taskStackSize(2048),
        messageQueueSize(16),
        taskTimeoutMs(30000),
        sessionTimeoutMs(300000),
        taskPriority(3),
        enableProgressReporting(true),
        enableToolsPagination(false) {}
};

// Transport interface for session communication
class SessionTransport {
public:
    virtual ~SessionTransport() = default;
    
    // Transport operations
    virtual int send(const std::string& data) = 0;
    virtual int receive(std::string& data, uint32_t timeoutMs = 1000) = 0;
    virtual bool isConnected() const = 0;
    virtual void close() = 0;
    
    // Transport info
    virtual std::string getClientInfo() const = 0;
    virtual size_t getMaxMessageSize() const { return 4096; }
};

// Async task base class
class AsyncTask {
public:
    AsyncTask(const MessageId& requestId, const std::string& method);
    virtual ~AsyncTask() = default;
    
    // Task lifecycle
    virtual int execute() = 0;
    virtual void cancel();
    virtual bool isFinished() const { return finished_; }
    virtual bool isCancelled() const { return cancelled_; }
    virtual bool isValid() const = 0;
    
    // Progress reporting
    virtual int reportProgress(int current, int total, const std::string& message = "");
    
    // Getters
    const MessageId& getRequestId() const { return requestId_; }
    const std::string& getMethod() const { return method_; }
    TickType_t getStartTime() const { return startTime_; }
    TickType_t getTimeout() const { return timeoutTicks_; }
    
    // Setters
    void setTimeout(uint32_t timeoutMs);
    void setProgressToken(const std::string& token) { progressToken_ = token; }
    
protected:
    MessageId requestId_;
    std::string method_;
    std::string progressToken_;
    std::atomic<bool> finished_;
    std::atomic<bool> cancelled_;
    TickType_t startTime_;
    TickType_t timeoutTicks_;
    SemaphoreHandle_t taskMutex_;
    
    // Helper for creating responses
    std::unique_ptr<Response> createResponse(const cJSON* result = nullptr);
    std::unique_ptr<Response> createErrorResponse(int errorCode, const std::string& message);
};

// Message context for processing
struct MessageContext {
    std::unique_ptr<Message> message;
    std::string rawJson;
    TickType_t receivedTime;
    bool requiresResponse;
    MessageId requestId;
    
    MessageContext(std::unique_ptr<Message> msg, const std::string& json) :
        message(std::move(msg)), rawJson(json), receivedTime(xTaskGetTickCount()),
        requiresResponse(false) {}
};

// Session statistics
struct SessionStats {
    uint32_t messagesReceived;
    uint32_t messagesSent;
    uint32_t tasksCreated;
    uint32_t tasksCompleted;
    uint32_t tasksCancelled;
    uint32_t errors;
    TickType_t sessionStartTime;
    TickType_t lastActivityTime;
    
    SessionStats() : messagesReceived(0), messagesSent(0), tasksCreated(0),
                    tasksCompleted(0), tasksCancelled(0), errors(0),
                    sessionStartTime(0), lastActivityTime(0) {}
};

// Main session class
class Session {
public:
    Session(std::unique_ptr<SessionTransport> transport, const SessionConfig& config = SessionConfig());
    ~Session();
    
    // Delete copy constructor and assignment
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    
    // Session lifecycle
    int initialize();
    int run();
    int shutdown();
    
    // State management
    SessionState getState() const { return state_; }
    bool isActive() const { return state_ == SessionState::ACTIVE; }
    bool isShuttingDown() const { return state_ == SessionState::SHUTTING_DOWN; }
    
    // Server information
    void setServerInfo(const std::string& name, const std::string& version);
    void setServerCapabilities(const ServerCapabilities& capabilities);
    void addTool(const std::string& name, const std::string& description, 
                 const cJSON* schema = nullptr);
    
    // Statistics
    const SessionStats& getStats() const { return stats_; }
    
    // Async task management
    int submitTask(std::unique_ptr<AsyncTask> task);
    int cancelTask(const MessageId& requestId);
    size_t getPendingTaskCount() const;
    
    // Message handling
    int sendMessage(const Message& message);
    int sendNotification(const std::string& method, const cJSON* params = nullptr);
    
private:
    // Core session tasks
    static void messageProcessorTask(void* pvParameters);
    static void asyncTaskManager(void* pvParameters);
    static void keepAliveTask(void* pvParameters);
    
    // Message processing
    int processMessage(std::unique_ptr<MessageContext> context);
    int processRequest(const Request& request);
    int processResponse(const Response& response);
    int processNotification(const Notification& notification);
    
    // Protocol handlers
    int handleInitializeRequest(const InitializeRequest& request);
    int handleListToolsRequest(const ListToolsRequest& request);
    int handleCallToolRequest(const CallToolRequest& request);
    int handleInitializedNotification(const InitializedNotification& notification);
    int handleCancelledNotification(const CancelledNotification& notification);
    
    // State transitions
    int transitionState(SessionState newState);
    bool canTransitionTo(SessionState newState) const;
    
    // Utility methods
    int sendResponse(const MessageId& requestId, const cJSON* result = nullptr);
    int sendErrorResponse(const MessageId& requestId, int errorCode, const std::string& message);
    void updateActivity();
    void cleanup();
    
    // Configuration and state
    SessionConfig config_;
    std::atomic<SessionState> state_;
    std::unique_ptr<SessionTransport> transport_;
    
    // Server information
    std::string serverName_;
    std::string serverVersion_;
    ServerCapabilities capabilities_;
    std::vector<std::string> supportedTools_;
    
    // FreeRTOS resources
    TaskHandle_t messageProcessorHandle_;
    TaskHandle_t asyncManagerHandle_;
    TaskHandle_t keepAliveHandle_;
    QueueHandle_t messageQueue_;
    QueueHandle_t taskQueue_;
    SemaphoreHandle_t sessionMutex_;
    EventGroupHandle_t sessionEvents_;
    
    // Event bits
    static const EventBits_t EVENT_SHUTDOWN_REQUEST = BIT0;
    static const EventBits_t EVENT_MESSAGE_RECEIVED = BIT1;
    static const EventBits_t EVENT_TASK_COMPLETED = BIT2;
    
    // Task management
    std::unordered_map<std::string, std::shared_ptr<AsyncTask>> pendingTasks_;
    std::deque<std::shared_ptr<AsyncTask>> completedTasks_;
    
    // Statistics
    SessionStats stats_;
    
    // Internal state
    bool initialized_;
    bool protocolInitialized_;
    TickType_t lastHeartbeat_;
};

// Session manager for handling multiple sessions
class SessionManager {
public:
    static SessionManager& getInstance();
    
    // Session management
    std::shared_ptr<Session> createSession(std::unique_ptr<SessionTransport> transport,
                                          const SessionConfig& config = SessionConfig());
    void removeSession(const std::shared_ptr<Session>& session);
    size_t getSessionCount() const;
    
    // Global operations
    void shutdownAll();
    void cleanupInactiveSessions();
    
    // Statistics
    struct GlobalStats {
        uint32_t totalSessionsCreated;
        uint32_t activeSessions;
        uint32_t totalMessages;
        uint32_t totalTasks;
        
        GlobalStats() : totalSessionsCreated(0), activeSessions(0),
                       totalMessages(0), totalTasks(0) {}
    };
    
    const GlobalStats& getGlobalStats() const { return globalStats_; }

private:
    SessionManager() = default;
    ~SessionManager() = default;
    
    // Singleton instance
    static SessionManager instance_;
    
    // Session storage
    std::vector<std::shared_ptr<Session>> sessions_;
    mutable SemaphoreHandle_t managerMutex_;
    
    // Statistics
    GlobalStats globalStats_;
    
    // Cleanup task
    TaskHandle_t cleanupTaskHandle_;
    static void cleanupTask(void* pvParameters);
};

// Utility functions
const char* sessionStateToString(SessionState state);
bool isValidSessionTransition(SessionState from, SessionState to);

// Tool execution tasks (to be extended by user)
class CallToolTask : public AsyncTask {
public:
    CallToolTask(const MessageId& requestId, const std::string& toolName, const cJSON* args);
    virtual ~CallToolTask() = default;
    
    bool isValid() const override;
    int execute() override;
    
protected:
    std::string toolName_;
    cJSON* arguments_;
    
    // Override this method to implement actual tool logic
    virtual int executeToolLogic(const cJSON* args, cJSON** result) = 0;
};

// Built-in error handling task
class ErrorTask : public AsyncTask {
public:
    ErrorTask(const MessageId& requestId, int errorCode, const std::string& errorMessage);
    
    bool isValid() const override;
    int execute() override;
    
private:
    int errorCode_;
    std::string errorMessage_;
};

} // namespace tinymcp