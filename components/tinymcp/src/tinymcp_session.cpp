// FreeRTOS Session Management Implementation for TinyMCP Protocol
// ESP8266/ESP32 optimized session handling with async task support

#include "tinymcp_session.h"
#include "tinymcp_json.h"

#include "esp_log.h"
#include "esp_system.h"
#include <cstring>
#include <algorithm>

static const char* TAG = "tinymcp_session";

namespace tinymcp {

// Session Manager singleton
SessionManager SessionManager::instance_;

// Utility function implementations
const char* sessionStateToString(SessionState state) {
    switch (state) {
        case SessionState::UNINITIALIZED: return "UNINITIALIZED";
        case SessionState::INITIALIZING: return "INITIALIZING";
        case SessionState::INITIALIZED: return "INITIALIZED";
        case SessionState::ACTIVE: return "ACTIVE";
        case SessionState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        case SessionState::SHUTDOWN: return "SHUTDOWN";
        case SessionState::ERROR_STATE: return "ERROR_STATE";
        default: return "UNKNOWN";
    }
}

bool isValidSessionTransition(SessionState from, SessionState to) {
    switch (from) {
        case SessionState::UNINITIALIZED:
            return to == SessionState::INITIALIZING || to == SessionState::ERROR_STATE;
        case SessionState::INITIALIZING:
            return to == SessionState::INITIALIZED || to == SessionState::ERROR_STATE;
        case SessionState::INITIALIZED:
            return to == SessionState::ACTIVE || to == SessionState::ERROR_STATE;
        case SessionState::ACTIVE:
            return to == SessionState::SHUTTING_DOWN || to == SessionState::ERROR_STATE;
        case SessionState::SHUTTING_DOWN:
            return to == SessionState::SHUTDOWN || to == SessionState::ERROR_STATE;
        case SessionState::SHUTDOWN:
            return false; // Terminal state
        case SessionState::ERROR_STATE:
            return to == SessionState::SHUTDOWN; // Can only recover by shutting down
        default:
            return false;
    }
}

// AsyncTask implementation
AsyncTask::AsyncTask(const MessageId& requestId, const std::string& method) :
    requestId_(requestId), method_(method), finished_(false), cancelled_(false),
    startTime_(xTaskGetTickCount()), timeoutTicks_(pdMS_TO_TICKS(30000)) {
    
    taskMutex_ = xSemaphoreCreateMutex();
    if (!taskMutex_) {
        ESP_LOGE(TAG, "Failed to create task mutex for request %s", requestId.asString().c_str());
    }
}

void AsyncTask::cancel() {
    if (taskMutex_ && xSemaphoreTake(taskMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        cancelled_ = true;
        finished_ = true;
        xSemaphoreGive(taskMutex_);
        ESP_LOGW(TAG, "Task cancelled for request %s", requestId_.asString().c_str());
    }
}

int AsyncTask::reportProgress(int current, int total, const std::string& message) {
    if (cancelled_ || finished_) {
        return TINYMCP_ERROR_CANCELLED;
    }
    
    if (progressToken_.empty()) {
        return TINYMCP_ERROR_NO_PROGRESS_TOKEN;
    }
    
    // Create progress notification
    cJSON* params = cJSON_CreateObject();
    if (!params) {
        return TINYMCP_ERROR_OUT_OF_MEMORY;
    }
    
    cJSON* progressTokenJson = cJSON_CreateString(progressToken_.c_str());
    cJSON* progressJson = cJSON_CreateNumber((double)current / total * 100.0);
    
    if (progressTokenJson && progressJson) {
        cJSON_AddItemToObject(params, "progressToken", progressTokenJson);
        cJSON_AddItemToObject(params, "progress", progressJson);
        
        if (!message.empty()) {
            cJSON* messageJson = cJSON_CreateString(message.c_str());
            if (messageJson) {
                cJSON_AddItemToObject(params, "message", messageJson);
            }
        }
    }
    
    // This would normally send the notification through the session
    // For now, just log it
    ESP_LOGI(TAG, "Progress %d/%d for request %s: %s", 
             current, total, requestId_.asString().c_str(), message.c_str());
    
    cJSON_Delete(params);
    return TINYMCP_SUCCESS;
}

void AsyncTask::setTimeout(uint32_t timeoutMs) {
    timeoutTicks_ = pdMS_TO_TICKS(timeoutMs);
}

std::unique_ptr<Response> AsyncTask::createResponse(const cJSON* result) {
    auto response = std::make_unique<Response>(requestId_);
    if (result) {
        response->setResult(result);
    }
    return response;
}

std::unique_ptr<Response> AsyncTask::createErrorResponse(int errorCode, const std::string& message) {
    auto response = std::make_unique<Response>(requestId_);
    Error error(errorCode, message);
    response->setError(error);
    return response;
}

// Session implementation
Session::Session(std::unique_ptr<SessionTransport> transport, const SessionConfig& config) :
    config_(config), state_(SessionState::UNINITIALIZED), transport_(std::move(transport)),
    serverName_("TinyMCP ESP8266"), serverVersion_("1.0.0"), initialized_(false),
    protocolInitialized_(false), lastHeartbeat_(0) {
    
    // Initialize FreeRTOS resources
    messageQueue_ = xQueueCreate(config_.messageQueueSize, sizeof(MessageContext*));
    taskQueue_ = xQueueCreate(config_.maxPendingTasks, sizeof(AsyncTask*));
    sessionMutex_ = xSemaphoreCreateRecursiveMutex();
    sessionEvents_ = xEventGroupCreate();
    
    if (!messageQueue_ || !taskQueue_ || !sessionMutex_ || !sessionEvents_) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS resources for session");
        state_ = SessionState::ERROR_STATE;
        return;
    }
    
    // Initialize statistics
    stats_.sessionStartTime = xTaskGetTickCount();
    stats_.lastActivityTime = stats_.sessionStartTime;
    
    ESP_LOGI(TAG, "Session created with client: %s", transport_->getClientInfo().c_str());
}

Session::~Session() {
    shutdown();
    cleanup();
}

int Session::initialize() {
    if (state_ != SessionState::UNINITIALIZED) {
        ESP_LOGW(TAG, "Session already initialized or in error state");
        return TINYMCP_ERROR_INVALID_STATE;
    }
    
    if (!transport_ || !transport_->isConnected()) {
        ESP_LOGE(TAG, "Transport not available or not connected");
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    transitionState(SessionState::INITIALIZING);
    
    // Create message processor task
    BaseType_t result = xTaskCreate(
        messageProcessorTask,
        "mcp_msg_proc",
        config_.taskStackSize,
        this,
        config_.taskPriority,
        &messageProcessorHandle_
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create message processor task");
        transitionState(SessionState::ERROR_STATE);
        return TINYMCP_ERROR_TASK_CREATION_FAILED;
    }
    
    // Create async task manager
    result = xTaskCreate(
        asyncTaskManager,
        "mcp_async_mgr",
        config_.taskStackSize,
        this,
        config_.taskPriority,
        &asyncManagerHandle_
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create async task manager");
        transitionState(SessionState::ERROR_STATE);
        return TINYMCP_ERROR_TASK_CREATION_FAILED;
    }
    
    // Create keep-alive task
    result = xTaskCreate(
        keepAliveTask,
        "mcp_keepalive",
        1024, // Smaller stack for keep-alive
        this,
        config_.taskPriority - 1,
        &keepAliveHandle_
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create keep-alive task");
        transitionState(SessionState::ERROR_STATE);
        return TINYMCP_ERROR_TASK_CREATION_FAILED;
    }
    
    initialized_ = true;
    transitionState(SessionState::INITIALIZED);
    
    ESP_LOGI(TAG, "Session initialized successfully");
    return TINYMCP_SUCCESS;
}

int Session::run() {
    if (state_ != SessionState::INITIALIZED) {
        ESP_LOGE(TAG, "Session not properly initialized");
        return TINYMCP_ERROR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting session run loop");
    
    std::string messageBuffer;
    messageBuffer.reserve(transport_->getMaxMessageSize());
    
    while (state_ != SessionState::SHUTDOWN && state_ != SessionState::ERROR_STATE) {
        // Check for shutdown request
        EventBits_t events = xEventGroupWaitBits(
            sessionEvents_,
            EVENT_SHUTDOWN_REQUEST,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(100)
        );
        
        if (events & EVENT_SHUTDOWN_REQUEST) {
            ESP_LOGI(TAG, "Shutdown requested");
            break;
        }
        
        // Receive message from transport
        messageBuffer.clear();
        int result = transport_->receive(messageBuffer, 1000);
        
        if (result == TINYMCP_SUCCESS && !messageBuffer.empty()) {
            updateActivity();
            stats_.messagesReceived++;
            
            // Parse message
            auto message = Message::createFromJson(messageBuffer);
            if (message) {
                auto context = std::make_unique<MessageContext>(std::move(message), messageBuffer);
                
                // Queue message for processing
                MessageContext* contextPtr = context.release();
                if (xQueueSend(messageQueue_, &contextPtr, pdMS_TO_TICKS(100)) != pdTRUE) {
                    ESP_LOGW(TAG, "Message queue full, dropping message");
                    delete contextPtr;
                    stats_.errors++;
                }
            } else {
                ESP_LOGW(TAG, "Failed to parse message: %s", messageBuffer.c_str());
                stats_.errors++;
            }
        } else if (result != TINYMCP_ERROR_TIMEOUT) {
            ESP_LOGE(TAG, "Transport receive error: %d", result);
            if (!transport_->isConnected()) {
                ESP_LOGI(TAG, "Transport disconnected");
                break;
            }
        }
        
        // Check for session timeout
        TickType_t now = xTaskGetTickCount();
        if ((now - stats_.lastActivityTime) > pdMS_TO_TICKS(config_.sessionTimeoutMs)) {
            ESP_LOGW(TAG, "Session timeout reached");
            break;
        }
    }
    
    ESP_LOGI(TAG, "Session run loop ended");
    return shutdown();
}

int Session::shutdown() {
    if (state_ == SessionState::SHUTDOWN) {
        return TINYMCP_SUCCESS;
    }
    
    ESP_LOGI(TAG, "Shutting down session");
    
    transitionState(SessionState::SHUTTING_DOWN);
    
    // Signal shutdown to all tasks
    xEventGroupSetBits(sessionEvents_, EVENT_SHUTDOWN_REQUEST);
    
    // Wait for tasks to finish with timeout
    const TickType_t shutdownTimeout = pdMS_TO_TICKS(5000);
    
    if (messageProcessorHandle_) {
        vTaskDelete(messageProcessorHandle_);
        messageProcessorHandle_ = nullptr;
    }
    
    if (asyncManagerHandle_) {
        vTaskDelete(asyncManagerHandle_);
        asyncManagerHandle_ = nullptr;
    }
    
    if (keepAliveHandle_) {
        vTaskDelete(keepAliveHandle_);
        keepAliveHandle_ = nullptr;
    }
    
    // Cancel all pending tasks
    for (auto& [id, task] : pendingTasks_) {
        task->cancel();
        stats_.tasksCancelled++;
    }
    pendingTasks_.clear();
    
    // Close transport
    if (transport_) {
        transport_->close();
    }
    
    transitionState(SessionState::SHUTDOWN);
    ESP_LOGI(TAG, "Session shutdown complete");
    
    return TINYMCP_SUCCESS;
}

void Session::setServerInfo(const std::string& name, const std::string& version) {
    if (xSemaphoreTakeRecursive(sessionMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        serverName_ = name;
        serverVersion_ = version;
        xSemaphoreGiveRecursive(sessionMutex_);
    }
}

void Session::setServerCapabilities(const ServerCapabilities& capabilities) {
    if (xSemaphoreTakeRecursive(sessionMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        capabilities_ = capabilities;
        xSemaphoreGiveRecursive(sessionMutex_);
    }
}

void Session::addTool(const std::string& name, const std::string& description, const cJSON* schema) {
    if (xSemaphoreTakeRecursive(sessionMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        supportedTools_.push_back(name);
        xSemaphoreGiveRecursive(sessionMutex_);
        ESP_LOGI(TAG, "Added tool: %s", name.c_str());
    }
}

int Session::submitTask(std::unique_ptr<AsyncTask> task) {
    if (!task || !task->isValid()) {
        return TINYMCP_ERROR_INVALID_PARAMS;
    }
    
    if (pendingTasks_.size() >= config_.maxPendingTasks) {
        ESP_LOGW(TAG, "Too many pending tasks");
        return TINYMCP_ERROR_RESOURCE_LIMIT;
    }
    
    if (xSemaphoreTakeRecursive(sessionMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        std::string taskId = task->getRequestId().asString();
        pendingTasks_[taskId] = std::shared_ptr<AsyncTask>(task.release());
        stats_.tasksCreated++;
        xSemaphoreGiveRecursive(sessionMutex_);
        
        ESP_LOGI(TAG, "Submitted task for request %s", taskId.c_str());
        return TINYMCP_SUCCESS;
    }
    
    return TINYMCP_ERROR_RESOURCE_LOCK;
}

int Session::cancelTask(const MessageId& requestId) {
    std::string taskId = requestId.asString();
    
    if (xSemaphoreTakeRecursive(sessionMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
        auto it = pendingTasks_.find(taskId);
        if (it != pendingTasks_.end()) {
            it->second->cancel();
            stats_.tasksCancelled++;
            xSemaphoreGiveRecursive(sessionMutex_);
            ESP_LOGI(TAG, "Cancelled task for request %s", taskId.c_str());
            return TINYMCP_SUCCESS;
        }
        xSemaphoreGiveRecursive(sessionMutex_);
    }
    
    return TINYMCP_ERROR_NOT_FOUND;
}

size_t Session::getPendingTaskCount() const {
    return pendingTasks_.size();
}

int Session::sendMessage(const Message& message) {
    std::string jsonStr;
    int result = message.serialize(jsonStr);
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    result = transport_->send(jsonStr);
    if (result == TINYMCP_SUCCESS) {
        stats_.messagesSent++;
        updateActivity();
    }
    
    return result;
}

int Session::sendNotification(const std::string& method, const cJSON* params) {
    Notification notification(method);
    if (params) {
        notification.setParams(params);
    }
    
    return sendMessage(notification);
}

// Static task functions
void Session::messageProcessorTask(void* pvParameters) {
    Session* session = static_cast<Session*>(pvParameters);
    MessageContext* context = nullptr;
    
    ESP_LOGI(TAG, "Message processor task started");
    
    while (session->state_ != SessionState::SHUTDOWN) {
        // Check for shutdown
        EventBits_t events = xEventGroupWaitBits(
            session->sessionEvents_,
            EVENT_SHUTDOWN_REQUEST,
            pdFALSE,
            pdFALSE,
            0
        );
        
        if (events & EVENT_SHUTDOWN_REQUEST) {
            break;
        }
        
        // Wait for messages
        if (xQueueReceive(session->messageQueue_, &context, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (context) {
                session->processMessage(std::unique_ptr<MessageContext>(context));
                context = nullptr;
            }
        }
    }
    
    ESP_LOGI(TAG, "Message processor task ended");
    vTaskDelete(NULL);
}

void Session::asyncTaskManager(void* pvParameters) {
    Session* session = static_cast<Session*>(pvParameters);
    
    ESP_LOGI(TAG, "Async task manager started");
    
    while (session->state_ != SessionState::SHUTDOWN) {
        // Check for shutdown
        EventBits_t events = xEventGroupWaitBits(
            session->sessionEvents_,
            EVENT_SHUTDOWN_REQUEST,
            pdFALSE,
            pdFALSE,
            0
        );
        
        if (events & EVENT_SHUTDOWN_REQUEST) {
            break;
        }
        
        // Process pending tasks
        if (xSemaphoreTakeRecursive(session->sessionMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            auto it = session->pendingTasks_.begin();
            while (it != session->pendingTasks_.end()) {
                auto& task = it->second;
                
                if (task->isCancelled() || task->isFinished()) {
                    // Move to completed tasks
                    session->completedTasks_.push_back(task);
                    if (task->isFinished() && !task->isCancelled()) {
                        session->stats_.tasksCompleted++;
                    }
                    it = session->pendingTasks_.erase(it);
                } else {
                    // Check for timeout
                    TickType_t now = xTaskGetTickCount();
                    if ((now - task->getStartTime()) > task->getTimeout()) {
                        ESP_LOGW(TAG, "Task timeout for request %s", 
                                task->getRequestId().asString().c_str());
                        task->cancel();
                        session->stats_.tasksCancelled++;
                    } else {
                        // Execute task (non-blocking)
                        task->execute();
                    }
                    ++it;
                }
            }
            
            // Cleanup completed tasks (keep last few for debugging)
            while (session->completedTasks_.size() > 10) {
                session->completedTasks_.pop_front();
            }
            
            xSemaphoreGiveRecursive(session->sessionMutex_);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to prevent busy waiting
    }
    
    ESP_LOGI(TAG, "Async task manager ended");
    vTaskDelete(NULL);
}

void Session::keepAliveTask(void* pvParameters) {
    Session* session = static_cast<Session*>(pvParameters);
    
    ESP_LOGI(TAG, "Keep-alive task started");
    
    while (session->state_ != SessionState::SHUTDOWN) {
        // Check for shutdown
        EventBits_t events = xEventGroupWaitBits(
            session->sessionEvents_,
            EVENT_SHUTDOWN_REQUEST,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(30000) // 30 second intervals
        );
        
        if (events & EVENT_SHUTDOWN_REQUEST) {
            break;
        }
        
        // Send ping if no recent activity
        TickType_t now = xTaskGetTickCount();
        if ((now - session->stats_.lastActivityTime) > pdMS_TO_TICKS(60000)) { // 1 minute
            session->sendNotification("notifications/ping");
            session->stats_.lastActivityTime = now;
        }
    }
    
    ESP_LOGI(TAG, "Keep-alive task ended");
    vTaskDelete(NULL);
}

int Session::processMessage(std::unique_ptr<MessageContext> context) {
    if (!context || !context->message) {
        return TINYMCP_ERROR_INVALID_PARAMS;
    }
    
    Message* msg = context->message.get();
    
    switch (msg->getCategory()) {
        case MessageCategory::REQUEST:
            return processRequest(static_cast<const Request&>(*msg));
        case MessageCategory::RESPONSE:
            return processResponse(static_cast<const Response&>(*msg));
        case MessageCategory::NOTIFICATION:
            return processNotification(static_cast<const Notification&>(*msg));
        default:
            ESP_LOGW(TAG, "Unknown message category");
            return TINYMCP_ERROR_INVALID_MESSAGE;
    }
}

int Session::processRequest(const Request& request) {
    const std::string& method = request.getMethod();
    
    ESP_LOGI(TAG, "Processing request: %s", method.c_str());
    
    if (method == "initialize") {
        return handleInitializeRequest(static_cast<const InitializeRequest&>(request));
    } else if (method == "tools/list") {
        return handleListToolsRequest(static_cast<const ListToolsRequest&>(request));
    } else if (method == "tools/call") {
        return handleCallToolRequest(static_cast<const CallToolRequest&>(request));
    } else {
        // Unknown method
        return sendErrorResponse(request.getId(), 
                               TINYMCP_ERROR_METHOD_NOT_FOUND,
                               "Method not found: " + method);
    }
}

int Session::processResponse(const Response& response) {
    ESP_LOGI(TAG, "Processing response for request: %s", response.getId().asString().c_str());
    // Handle responses if needed
    return TINYMCP_SUCCESS;
}

int Session::processNotification(const Notification& notification) {
    const std::string& method = notification.getMethod();
    
    ESP_LOGI(TAG, "Processing notification: %s", method.c_str());
    
    if (method == "initialized") {
        return handleInitializedNotification(static_cast<const InitializedNotification&>(notification));
    } else if (method == "notifications/cancelled") {
        return handleCancelledNotification(static_cast<const CancelledNotification&>(notification));
    }
    
    return TINYMCP_SUCCESS;
}

int Session::handleInitializeRequest(const InitializeRequest& request) {
    if (state_ != SessionState::INITIALIZED) {
        return sendErrorResponse(request.getId(), 
                               TINYMCP_ERROR_INVALID_STATE,
                               "Session not in correct state for initialization");
    }
    
    // Create server info and capabilities
    cJSON* result = cJSON_CreateObject();
    if (!result) {
        return TINYMCP_ERROR_OUT_OF_MEMORY;
    }
    
    // Protocol version
    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
    
    // Server info
    cJSON* serverInfo = cJSON_CreateObject();
    cJSON_AddStringToObject(serverInfo, "name", serverName_.c_str());
    cJSON_AddStringToObject(serverInfo, "version", serverVersion_.c_str());
    cJSON_AddItemToObject(result, "serverInfo", serverInfo);
    
    // Capabilities
    cJSON* capabilities = capabilities_.toJson();
    cJSON_AddItemToObject(result, "capabilities", capabilities);
    
    int sendResult = sendResponse(request.getId(), result);
    cJSON_Delete(result);
    
    if (sendResult == TINYMCP_SUCCESS) {
        // Wait for initialized notification
        protocolInitialized_ = true;
    }
    
    return sendResult;
}

int Session::handleListToolsRequest(const ListToolsRequest& request) {
    if (!protocolInitialized_) {
        return sendErrorResponse(request.getId(), 
                               TINYMCP_ERROR_NOT_INITIALIZED,
                               "Session not initialized");
    }
    
    cJSON* result = cJSON_CreateObject();
    cJSON* tools = cJSON_CreateArray();
    
    if (!result || !tools) {
        cJSON_Delete(result);
        cJSON_Delete(tools);
        return TINYMCP_ERROR_OUT_OF_MEMORY;
    }
    
    // Add supported tools
    for (const auto& toolName : supportedTools_) {
        cJSON* tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", toolName.c_str());
        cJSON_AddStringToObject(tool, "description", "Tool description");
        cJSON_AddItemToArray(tools, tool);
    }
    
    cJSON_AddItemToObject(result, "tools", tools);
    
    int sendResult = sendResponse(request.getId(), result);
    cJSON_Delete(result);
    
    return sendResult;
}

int Session::handleCallToolRequest(const CallToolRequest& request) {
    if (!protocolInitialized_) {
        return sendErrorResponse(request.getId(), 
                               TINYMCP_ERROR_NOT_INITIALIZED,
                               "Session not initialized");
    }
    
    const std::string& toolName = request.getToolName();
    
    // Check if tool is supported
    auto it = std::find(supportedTools_.begin(), supportedTools_.end(), toolName);
    if (it == supportedTools_.end()) {
        return sendErrorResponse(request.getId(), 
                               TINYMCP_ERROR_TOOL_NOT_FOUND,
                               "Tool not found: " + toolName);
    }
    
    // Create async task for tool execution
    auto task = std::make_unique<CallToolTask>(request.getId(), toolName, request.getArguments());
    
    return submitTask(std::move(task));
}

int Session::handleInitializedNotification(const InitializedNotification& notification) {
    ESP_LOGI(TAG, "Client initialized notification received");
    transitionState(SessionState::ACTIVE);
    return TINYMCP_SUCCESS;
}

int Session::handleCancelledNotification(const CancelledNotification& notification) {
    // Handle cancellation requests
    return TINYMCP_SUCCESS;
}

int Session::transitionState(SessionState newState) {
    if (!isValidSessionTransition(state_, newState)) {
        ESP_LOGW(TAG, "Invalid state transition from %s to %s", 
                sessionStateToString(state_), sessionStateToString(newState));
        return TINYMCP_ERROR_INVALID_STATE;
    }
    
    SessionState oldState = state_;
    state_ = newState;
    
    ESP_LOGI(TAG, "Session state: %s -> %s", 
            sessionStateToString(oldState), sessionStateToString(newState));
    
    return TINYMCP_SUCCESS;
}

bool Session::canTransitionTo(SessionState newState) const {
    return isValidSessionTransition(state_, newState);
}

int Session::sendResponse(const MessageId& requestId, const cJSON* result) {
    Response response(requestId);
    if (result) {
        response.setResult(result);
    }
    return sendMessage(response);
}

int Session::sendErrorResponse(const MessageId& requestId, int errorCode, const std::string& message) {
    Response response(requestId);
    Error error(errorCode, message);
    response.setError(error);
    return sendMessage(response);
}

void Session::updateActivity() {
    stats_.lastActivityTime = xTaskGetTickCount();
}

void Session::cleanup() {
    // Clean up FreeRTOS resources
    if (messageQueue_) {
        vQueueDelete(messageQueue_);
        messageQueue_ = nullptr;
    }
    
    if (taskQueue_) {
        vQueueDelete(taskQueue_);
        taskQueue_ = nullptr;
    }
    
    if (sessionMutex_) {
        vSemaphoreDelete(sessionMutex_);
        sessionMutex_ = nullptr;
    }
    
    if (sessionEvents_) {
        vEventGroupDelete(sessionEvents_);
        sessionEvents_ = nullptr;
    }
}

// SessionManager implementation
SessionManager& SessionManager::getInstance() {
    return instance_;
}

std::shared_ptr<Session> SessionManager::createSession(
    std::unique_ptr<SessionTransport> transport,
    const SessionConfig& config) {
    
    auto session = std::shared_ptr<Session>(new Session(std::move(transport), config));
    
    if (session->getState() != SessionState::ERROR_STATE) {
        sessions_.push_back(session);
        globalStats_.totalSessionsCreated++;
        globalStats_.activeSessions++;
    }
    
    return session;
}

void SessionManager::removeSession(const std::shared_ptr<Session>& session) {
    auto it = std::find(sessions_.begin(), sessions_.end(), session);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        globalStats_.activeSessions--;
    }
}

size_t SessionManager::getSessionCount() const {
    return sessions_.size();
}

void SessionManager::shutdownAll() {
    for (auto& session : sessions_) {
        session->shutdown();
    }
    sessions_.clear();
    globalStats_.activeSessions = 0;
}

void SessionManager::cleanupInactiveSessions() {
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if ((*it)->getState() == SessionState::SHUTDOWN || 
            (*it)->getState() == SessionState::ERROR_STATE) {
            it = sessions_.erase(it);
            globalStats_.activeSessions--;
        } else {
            ++it;
        }
    }
}

// CallToolTask implementation
CallToolTask::CallToolTask(const MessageId& requestId, const std::string& toolName, const cJSON* args) :
    AsyncTask(requestId, "tools/call"), toolName_(toolName), arguments_(nullptr) {
    
    if (args) {
        arguments_ = cJSON_Duplicate(args, cJSON_True);
    }
}

bool CallToolTask::isValid() const {
    return !toolName_.empty();
}

int CallToolTask::execute() {
    if (cancelled_ || finished_) {
        return TINYMCP_ERROR_CANCELLED;
    }
    
    ESP_LOGI(TAG, "Executing tool: %s", toolName_.c_str());
    
    cJSON* result = nullptr;
    int executeResult = executeToolLogic(arguments_, &result);
    
    if (executeResult == TINYMCP_SUCCESS) {
        finished_ = true;
        // Tool execution successful - result would be sent by derived class
        ESP_LOGI(TAG, "Tool %s executed successfully", toolName_.c_str());
    } else {
        finished_ = true;
        ESP_LOGE(TAG, "Tool %s execution failed: %d", toolName_.c_str(), executeResult);
    }
    
    if (result) {
        cJSON_Delete(result);
    }
    
    return executeResult;
}

// ErrorTask implementation
ErrorTask::ErrorTask(const MessageId& requestId, int errorCode, const std::string& errorMessage) :
    AsyncTask(requestId, "error"), errorCode_(errorCode), errorMessage_(errorMessage) {
}

bool ErrorTask::isValid() const {
    return errorCode_ != 0 && !errorMessage_.empty();
}

int ErrorTask::execute() {
    if (cancelled_ || finished_) {
        return TINYMCP_ERROR_CANCELLED;
    }
    
    ESP_LOGE(TAG, "Error task executing: %d - %s", errorCode_, errorMessage_.c_str());
    
    finished_ = true;
    return TINYMCP_SUCCESS;
}

} // namespace tinymcp