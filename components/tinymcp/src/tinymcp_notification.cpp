// Notification Message Classes for TinyMCP Protocol
// Implementation of JSON-RPC 2.0 compliant notification messages for ESP32/ESP8266

#include "tinymcp_notification.h"
#include <algorithm>
#include <cstring>

namespace tinymcp {

// Notification implementation

Notification::Notification(MessageType type, const std::string& method)
    : Message(type, MessageCategory::NOTIFICATION)
    , method_(method) {
}

bool Notification::isValid() const {
    return !method_.empty() && method_.length() <= MAX_METHOD_NAME_LENGTH;
}

int Notification::serialize(std::string& jsonOut) const {
    JsonValue json = JsonValue::createObject();
    if (!json.isValid()) return -1;
    
    int result = doSerialize(json.get());
    if (result != 0) return result;
    
    jsonOut = JsonHelper::toString(json.get());
    return jsonOut.empty() ? -1 : 0;
}

int Notification::deserialize(const std::string& jsonIn) {
    JsonValue json = JsonValue::parse(jsonIn);
    if (!json.isValid()) return TINYMCP_PARSE_ERROR;
    
    return doDeserialize(json.get());
}

int Notification::doSerialize(cJSON* json) const {
    if (!json) return -1;
    
    // Add common JSON-RPC fields
    if (!addCommonFields(json)) return -1;
    
    // Add method
    if (!JsonHelper::setString(json, MSG_KEY_METHOD, method_)) return -1;
    
    // Add parameters (implemented by derived classes)
    int result = serializeParams(json);
    if (result != 0) return result;
    
    return 0;
}

int Notification::doDeserialize(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    // Validate common fields
    if (!validateCommonFields(json)) return TINYMCP_INVALID_REQUEST;
    
    // Validate notification structure (no ID field allowed)
    if (JsonHelper::hasField(json, MSG_KEY_ID)) {
        return TINYMCP_INVALID_REQUEST;
    }
    
    // Validate and extract method
    if (!JsonHelper::isString(json, MSG_KEY_METHOD)) {
        return TINYMCP_INVALID_REQUEST;
    }
    method_ = JsonHelper::getString(json, MSG_KEY_METHOD);
    
    // Extract progress token if present
    if (JsonHelper::hasField(json, MSG_KEY_PROGRESS_TOKEN)) {
        setProgressToken(JsonHelper::getString(json, MSG_KEY_PROGRESS_TOKEN));
    }
    
    // Deserialize parameters (implemented by derived classes)
    cJSON* params = JsonHelper::getObject(json, MSG_KEY_PARAMS);
    if (params) {
        int result = deserializeParams(params);
        if (result != 0) return result;
    }
    
    return 0;
}

// InitializedNotification implementation

void InitializedNotification::setClientCapabilities(cJSON* capabilities) {
    if (clientCapabilities_) {
        cJSON_Delete(clientCapabilities_);
    }
    clientCapabilities_ = capabilities;
}

bool InitializedNotification::validateParams(const cJSON* params) const {
    // All parameters are optional for initialized notification
    if (!params) return true;
    
    // If clientVersion is present, it must be a string
    if (JsonHelper::hasField(params, MSG_KEY_VERSION)) {
        if (!JsonHelper::isString(params, MSG_KEY_VERSION)) return false;
    }
    
    // If sessionId is present, it must be a string
    if (JsonHelper::hasField(params, "sessionId")) {
        if (!JsonHelper::isString(params, "sessionId")) return false;
    }
    
    // If capabilities are present, they must be an object
    if (JsonHelper::hasField(params, MSG_KEY_CAPABILITIES)) {
        if (!JsonHelper::isObject(params, MSG_KEY_CAPABILITIES)) return false;
    }
    
    return true;
}

int InitializedNotification::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    // Only add params if we have any data
    if (clientVersion_.empty() && sessionId_.empty() && !clientCapabilities_) {
        return 0; // No parameters needed
    }
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    if (!clientVersion_.empty()) {
        if (!JsonHelper::setString(params, MSG_KEY_VERSION, clientVersion_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    if (!sessionId_.empty()) {
        if (!JsonHelper::setString(params, "sessionId", sessionId_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    if (clientCapabilities_) {
        cJSON* capabilitiesCopy = JsonHelper::safeDuplicate(clientCapabilities_);
        if (capabilitiesCopy) {
            JsonHelper::setObject(params, MSG_KEY_CAPABILITIES, capabilitiesCopy);
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int InitializedNotification::deserializeParams(const cJSON* json) {
    if (!json) return 0; // No parameters is valid
    
    clientVersion_ = JsonHelper::getString(json, MSG_KEY_VERSION);
    sessionId_ = JsonHelper::getString(json, "sessionId");
    
    // Extract client capabilities if present
    if (JsonHelper::hasField(json, MSG_KEY_CAPABILITIES)) {
        cJSON* capabilities = JsonHelper::getObject(json, MSG_KEY_CAPABILITIES);
        if (capabilities) {
            setClientCapabilities(JsonHelper::safeDuplicate(capabilities));
        }
    }
    
    return 0;
}

// ProgressNotification implementation

bool ProgressNotification::validateParams(const cJSON* params) const {
    if (!params) return false;
    
    // Progress token is required
    if (!JsonHelper::isString(params, MSG_KEY_PROGRESS_TOKEN)) {
        return false;
    }
    
    // Progress must be a number
    if (!JsonHelper::isNumber(params, MSG_KEY_PROGRESS)) {
        return false;
    }
    
    // Total must be a number if present
    if (JsonHelper::hasField(params, MSG_KEY_TOTAL)) {
        if (!JsonHelper::isNumber(params, MSG_KEY_TOTAL)) {
            return false;
        }
    }
    
    return true;
}

int ProgressNotification::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    // Add progress token
    if (!JsonHelper::setString(params, MSG_KEY_PROGRESS_TOKEN, progressToken_.get())) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add progress and total
    if (!JsonHelper::setInt(params, MSG_KEY_PROGRESS, progress_)) {
        cJSON_Delete(params);
        return -1;
    }
    
    if (!JsonHelper::setInt(params, MSG_KEY_TOTAL, total_)) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add optional message
    if (!message_.empty()) {
        if (!JsonHelper::setString(params, MSG_KEY_MESSAGE, message_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    // Add optional details
    if (!details_.empty()) {
        if (!JsonHelper::setString(params, "details", details_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int ProgressNotification::deserializeParams(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_PARAMS;
    
    // Extract progress token
    std::string tokenStr = JsonHelper::getString(json, MSG_KEY_PROGRESS_TOKEN);
    if (tokenStr.empty()) return TINYMCP_INVALID_PARAMS;
    progressToken_ = ProgressToken(tokenStr);
    
    // Extract progress and total
    progress_ = JsonHelper::getInt(json, MSG_KEY_PROGRESS);
    total_ = JsonHelper::getInt(json, MSG_KEY_TOTAL, 100);
    
    // Extract optional fields
    message_ = JsonHelper::getString(json, MSG_KEY_MESSAGE);
    details_ = JsonHelper::getString(json, "details");
    
    return 0;
}

// CancelledNotification implementation

bool CancelledNotification::validateParams(const cJSON* params) const {
    if (!params) return false;
    
    // Request ID is required
    if (!JsonHelper::isString(params, "requestId")) {
        return false;
    }
    
    // Reason is optional but must be string if present
    if (JsonHelper::hasField(params, "reason")) {
        if (!JsonHelper::isString(params, "reason")) return false;
    }
    
    // Progress token is optional but must be string if present
    if (JsonHelper::hasField(params, MSG_KEY_PROGRESS_TOKEN)) {
        if (!JsonHelper::isString(params, MSG_KEY_PROGRESS_TOKEN)) return false;
    }
    
    return true;
}

int CancelledNotification::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    // Add request ID
    if (!JsonHelper::setString(params, "requestId", requestId_)) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add optional reason
    if (!reason_.empty()) {
        if (!JsonHelper::setString(params, "reason", reason_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    // Add optional progress token
    if (progressToken_.isValid()) {
        if (!JsonHelper::setString(params, MSG_KEY_PROGRESS_TOKEN, progressToken_.get())) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    // Add error information if present
    if (errorCode_ != 0) {
        cJSON* error = cJSON_CreateObject();
        if (error) {
            JsonHelper::setInt(error, MSG_KEY_CODE, errorCode_);
            JsonHelper::setString(error, MSG_KEY_MESSAGE, errorMessage_);
            JsonHelper::setObject(params, MSG_KEY_ERROR, error);
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int CancelledNotification::deserializeParams(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_PARAMS;
    
    // Extract request ID
    requestId_ = JsonHelper::getString(json, "requestId");
    if (requestId_.empty()) return TINYMCP_INVALID_PARAMS;
    
    // Extract optional fields
    reason_ = JsonHelper::getString(json, "reason");
    
    std::string tokenStr = JsonHelper::getString(json, MSG_KEY_PROGRESS_TOKEN);
    if (!tokenStr.empty()) {
        progressToken_ = ProgressToken(tokenStr);
    }
    
    // Extract error information if present
    if (JsonHelper::hasField(json, MSG_KEY_ERROR)) {
        cJSON* error = JsonHelper::getObject(json, MSG_KEY_ERROR);
        if (error) {
            errorCode_ = JsonHelper::getInt(error, MSG_KEY_CODE);
            errorMessage_ = JsonHelper::getString(error, MSG_KEY_MESSAGE);
        }
    }
    
    return 0;
}

// ToolsListChangedNotification implementation

bool ToolsListChangedNotification::validateParams(const cJSON* params) const {
    // All parameters are optional
    if (!params) return true;
    
    // If tool lists are present, they must be arrays
    if (JsonHelper::hasField(params, "added")) {
        if (!JsonHelper::isArray(params, "added")) return false;
    }
    
    if (JsonHelper::hasField(params, "removed")) {
        if (!JsonHelper::isArray(params, "removed")) return false;
    }
    
    if (JsonHelper::hasField(params, "modified")) {
        if (!JsonHelper::isArray(params, "modified")) return false;
    }
    
    return true;
}

int ToolsListChangedNotification::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    // Only add params if we have changes
    if (!hasChanges()) {
        return 0;
    }
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    // Add added tools
    if (!addedTools_.empty()) {
        cJSON* added = cJSON_CreateArray();
        if (added) {
            for (const auto& tool : addedTools_) {
                cJSON* toolName = cJSON_CreateString(tool.c_str());
                if (toolName) {
                    cJSON_AddItemToArray(added, toolName);
                }
            }
            JsonHelper::setArray(params, "added", added);
        }
    }
    
    // Add removed tools
    if (!removedTools_.empty()) {
        cJSON* removed = cJSON_CreateArray();
        if (removed) {
            for (const auto& tool : removedTools_) {
                cJSON* toolName = cJSON_CreateString(tool.c_str());
                if (toolName) {
                    cJSON_AddItemToArray(removed, toolName);
                }
            }
            JsonHelper::setArray(params, "removed", removed);
        }
    }
    
    // Add modified tools
    if (!modifiedTools_.empty()) {
        cJSON* modified = cJSON_CreateArray();
        if (modified) {
            for (const auto& tool : modifiedTools_) {
                cJSON* toolName = cJSON_CreateString(tool.c_str());
                if (toolName) {
                    cJSON_AddItemToArray(modified, toolName);
                }
            }
            JsonHelper::setArray(params, "modified", modified);
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int ToolsListChangedNotification::deserializeParams(const cJSON* json) {
    if (!json) return 0; // No parameters is valid
    
    // Extract tool lists
    auto extractToolList = [](const cJSON* params, const char* key, std::vector<std::string>& tools) {
        cJSON* array = JsonHelper::getArray(params, key);
        if (array) {
            int size = JsonHelper::getArraySize(array);
            for (int i = 0; i < size; i++) {
                cJSON* item = JsonHelper::getArrayItem(array, i);
                if (item && cJSON_IsString(item)) {
                    const char* toolName = cJSON_GetStringValue(item);
                    if (toolName) {
                        tools.emplace_back(toolName);
                    }
                }
            }
        }
    };
    
    extractToolList(json, "added", addedTools_);
    extractToolList(json, "removed", removedTools_);
    extractToolList(json, "modified", modifiedTools_);
    
    return 0;
}

// LogNotification implementation

void LogNotification::setData(cJSON* data) {
    if (data_) {
        cJSON_Delete(data_);
    }
    data_ = data;
}

bool LogNotification::validateParams(const cJSON* params) const {
    if (!params) return false;
    
    // Level is required
    if (!JsonHelper::isString(params, "level")) {
        return false;
    }
    
    // Message is required
    if (!JsonHelper::isString(params, MSG_KEY_MESSAGE)) {
        return false;
    }
    
    return true;
}

int LogNotification::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    // Add level
    if (!JsonHelper::setString(params, "level", getLevelString())) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add message
    if (!JsonHelper::setString(params, MSG_KEY_MESSAGE, message_)) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add optional context
    if (!context_.empty()) {
        if (!JsonHelper::setString(params, "context", context_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    // Add optional data
    if (data_) {
        cJSON* dataCopy = JsonHelper::safeDuplicate(data_);
        if (dataCopy) {
            JsonHelper::setObject(params, MSG_KEY_DATA, dataCopy);
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int LogNotification::deserializeParams(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_PARAMS;
    
    // Extract level
    std::string levelStr = JsonHelper::getString(json, "level");
    if (levelStr == "debug") level_ = DEBUG;
    else if (levelStr == "info") level_ = INFO;
    else if (levelStr == "warn") level_ = WARN;
    else if (levelStr == "error") level_ = ERROR;
    else return TINYMCP_INVALID_PARAMS;
    
    // Extract message
    message_ = JsonHelper::getString(json, MSG_KEY_MESSAGE);
    if (message_.empty()) return TINYMCP_INVALID_PARAMS;
    
    // Extract optional fields
    context_ = JsonHelper::getString(json, "context");
    
    if (JsonHelper::hasField(json, MSG_KEY_DATA)) {
        cJSON* data = JsonHelper::getObject(json, MSG_KEY_DATA);
        if (data) {
            setData(JsonHelper::safeDuplicate(data));
        }
    }
    
    return 0;
}

// NotificationFactory implementation

std::unique_ptr<Notification> NotificationFactory::createFromJson(const std::string& jsonStr) {
    JsonValue json = JsonValue::parse(jsonStr);
    if (!json.isValid()) return nullptr;
    
    return createFromJson(json.get());
}

std::unique_ptr<Notification> NotificationFactory::createFromJson(const cJSON* json) {
    if (!json || !JsonHelper::validateNotification(json)) return nullptr;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    MessageType type = getNotificationType(method);
    
    if (type == MessageType::UNKNOWN) return nullptr;
    
    std::unique_ptr<Notification> notification;
    
    switch (type) {
        case MessageType::INITIALIZED_NOTIFICATION: {
            auto initNotif = std::make_unique<InitializedNotification>();
            if (initNotif->deserialize(JsonHelper::toString(json)) == 0) {
                notification = std::move(initNotif);
            }
            break;
        }
        case MessageType::PROGRESS_NOTIFICATION: {
            auto progNotif = std::make_unique<ProgressNotification>(ProgressToken(), 0, 100);
            if (progNotif->deserialize(JsonHelper::toString(json)) == 0) {
                notification = std::move(progNotif);
            }
            break;
        }
        case MessageType::CANCELLED_NOTIFICATION: {
            auto cancelNotif = std::make_unique<CancelledNotification>("", "");
            if (cancelNotif->deserialize(JsonHelper::toString(json)) == 0) {
                notification = std::move(cancelNotif);
            }
            break;
        }
        default:
            break;
    }
    
    return notification;
}

std::unique_ptr<InitializedNotification> NotificationFactory::createInitializedNotification(
    const std::string& clientVersion,
    const std::string& sessionId) {
    
    auto notification = std::make_unique<InitializedNotification>();
    if (!clientVersion.empty()) notification->setClientVersion(clientVersion);
    if (!sessionId.empty()) notification->setSessionId(sessionId);
    return notification;
}

std::unique_ptr<ProgressNotification> NotificationFactory::createProgressNotification(
    const ProgressToken& token,
    int progress,
    int total,
    const std::string& message) {
    
    auto notification = std::make_unique<ProgressNotification>(token, progress, total);
    if (!message.empty()) notification->setMessage(message);
    return notification;
}

std::unique_ptr<CancelledNotification> NotificationFactory::createCancelledNotification(
    const std::string& requestId,
    const std::string& reason) {
    
    return std::make_unique<CancelledNotification>(requestId, reason);
}

std::unique_ptr<ToolsListChangedNotification> NotificationFactory::createToolsListChangedNotification() {
    return std::make_unique<ToolsListChangedNotification>();
}

std::unique_ptr<LogNotification> NotificationFactory::createLogNotification(
    LogNotification::LogLevel level,
    const std::string& message,
    const std::string& context) {
    
    auto notification = std::make_unique<LogNotification>(level, message);
    if (!context.empty()) notification->setContext(context);
    return notification;
}

MessageType NotificationFactory::getNotificationType(const std::string& method) {
    if (method == METHOD_INITIALIZED) return MessageType::INITIALIZED_NOTIFICATION;
    if (method == METHOD_PROGRESS) return MessageType::PROGRESS_NOTIFICATION;
    if (method == METHOD_CANCELLED) return MessageType::CANCELLED_NOTIFICATION;
    return MessageType::UNKNOWN;
}

// NotificationValidator implementation

bool NotificationValidator::validateInitializedNotification(const cJSON* json) {
    if (!JsonHelper::validateNotification(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    return method == METHOD_INITIALIZED;
}

bool NotificationValidator::validateProgressNotification(const cJSON* json) {
    if (!JsonHelper::validateNotification(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    if (method != METHOD_PROGRESS) return false;
    
    cJSON* params = JsonHelper::getObject(json, MSG_KEY_PARAMS);
    if (!params) return false;
    
    // Validate required fields
    if (!validateProgressToken(JsonHelper::getString(params, MSG_KEY_PROGRESS_TOKEN))) {
        return false;
    }
    
    int progress = JsonHelper::getInt(params, MSG_KEY_PROGRESS);
    int total = JsonHelper::getInt(params, MSG_KEY_TOTAL, 100);
    
    return isValidProgressValue(progress, total);
}

bool NotificationValidator::validateCancelledNotification(const cJSON* json) {
    if (!JsonHelper::validateNotification(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    if (method != METHOD_CANCELLED) return false;
    
    cJSON* params = JsonHelper::getObject(json, MSG_KEY_PARAMS);
    if (!params) return false;
    
    return validateRequestId(JsonHelper::getString(params, "requestId"));
}

bool NotificationValidator::validateToolsListChangedNotification(const cJSON* json) {
    if (!JsonHelper::validateNotification(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    return method == "notifications/tools/list_changed";
}

bool NotificationValidator::validateLogNotification(const cJSON* json) {
    if (!JsonHelper::validateNotification(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    if (method != "notifications/log") return false;
    
    cJSON* params = JsonHelper::getObject(json, MSG_KEY_PARAMS);
    if (!params) return false;
    
    std::string level = JsonHelper::getString(params, "level");
    std::string message = JsonHelper::getString(params, MSG_KEY_MESSAGE);
    
    return validateLogLevel(level) && !message.empty();
}

bool NotificationValidator::validateProgressToken(const std::string& token) {
    return !token.empty() && token.length() <= 64;
}

bool NotificationValidator::validateRequestId(const std::string& requestId) {
    return !requestId.empty() && requestId.length() <= 64;
}

bool NotificationValidator::validateLogLevel(const std::string& level) {
    return level == "debug" || level == "info" || level == "warn" || level == "error";
}

bool NotificationValidator::validateToolNameList(const cJSON* toolList) {
    if (!toolList || !cJSON_IsArray(toolList)) return false;
    
    int size = JsonHelper::getArraySize(toolList);
    for (int i = 0; i < size; i++) {
        cJSON* item = JsonHelper::getArrayItem(toolList, i);
        if (!item || !cJSON_IsString(item)) return false;
        
        const char* toolName = cJSON_GetStringValue(item);
        if (!toolName || strlen(toolName) == 0 || strlen(toolName) > MAX_TOOL_NAME_LENGTH) {
            return false;
        }
    }
    
    return true;
}

bool NotificationValidator::isValidProgressValue(int progress, int total) {
    return progress >= 0 && total > 0 && progress <= total;
}

bool NotificationValidator::isValidLogLevel(int level) {
    return level >= 0 && level <= 3;
}

// NotificationBuilder implementation

NotificationBuilder& NotificationBuilder::initializedNotification() {
    notificationType_ = MessageType::INITIALIZED_NOTIFICATION;
    method_ = METHOD_INITIALIZED;
    return *this;
}

NotificationBuilder& NotificationBuilder::withClientVersion(const std::string& version) {
    clientVersion_ = version;
    return *this;
}

NotificationBuilder& NotificationBuilder::withSessionId(const std::string& sessionId) {
    sessionId_ = sessionId;
    return *this;
}

NotificationBuilder& NotificationBuilder::withClientCapabilities(cJSON* capabilities) {
    if (clientCapabilities_) {
        cJSON_Delete(clientCapabilities_);
    }
    clientCapabilities_ = capabilities;
    return *this;
}

NotificationBuilder& NotificationBuilder::progressNotification(const ProgressToken& token) {
    notificationType_ = MessageType::PROGRESS_NOTIFICATION;
    method_ = METHOD_PROGRESS;
    progressToken_ = token;
    return *this;
}

NotificationBuilder& NotificationBuilder::withProgress(int progress, int total) {
    progress_ = progress;
    total_ = total;
    return *this;
}

NotificationBuilder& NotificationBuilder::withProgressMessage(const std::string& message) {
    progressMessage_ = message;
    return *this;
}

NotificationBuilder& NotificationBuilder::withProgressDetails(const std::string& details) {
    progressDetails_ = details;
    return *this;
}

NotificationBuilder& NotificationBuilder::cancelledNotification(const std::string& requestId) {
    notificationType_ = MessageType::CANCELLED_NOTIFICATION;
    method_ = METHOD_CANCELLED;
    requestId_ = requestId;
    return *this;
}

NotificationBuilder& NotificationBuilder::withCancellationReason(const std::string& reason) {
    cancellationReason_ = reason;
    return *this;
}

NotificationBuilder& NotificationBuilder::withProgressToken(const ProgressToken& token) {
    progressToken_ = token;
    return *this;
}

NotificationBuilder& NotificationBuilder::withCancellationError(int code, const std::string& message) {
    errorCode_ = code;
    errorMessage_ = message;
    return *this;
}

NotificationBuilder& NotificationBuilder::toolsListChangedNotification() {
    notificationType_ = MessageType::UNKNOWN; // Special handling needed
    method_ = "notifications/tools/list_changed";
    return *this;
}

NotificationBuilder& NotificationBuilder::withAddedTools(const std::vector<std::string>& tools) {
    addedTools_ = tools;
    return *this;
}

NotificationBuilder& NotificationBuilder::withRemovedTools(const std::vector<std::string>& tools) {
    removedTools_ = tools;
    return *this;
}

NotificationBuilder& NotificationBuilder::withModifiedTools(const std::vector<std::string>& tools) {
    modifiedTools_ = tools;
    return *this;
}

NotificationBuilder& NotificationBuilder::logNotification(LogNotification::LogLevel level, const std::string& message) {
    notificationType_ = MessageType::UNKNOWN; // Special handling needed
    method_ = "notifications/log";
    logLevel_ = level;
    logMessage_ = message;
    return *this;
}

NotificationBuilder& NotificationBuilder::withLogContext(const std::string& context) {
    logContext_ = context;
    return *this;
}

NotificationBuilder& NotificationBuilder::withLogData(cJSON* data) {
    if (logData_) {
        cJSON_Delete(logData_);
    }
    logData_ = data;
    return *this;
}

std::unique_ptr<Notification> NotificationBuilder::build() {
    std::unique_ptr<Notification> notification;
    
    switch (notificationType_) {
        case MessageType::INITIALIZED_NOTIFICATION: {
            auto initNotif = std::make_unique<InitializedNotification>();
            initNotif->setClientVersion(clientVersion_);
            initNotif->setSessionId(sessionId_);
            if (clientCapabilities_) {
                initNotif->setClientCapabilities(clientCapabilities_);
                clientCapabilities_ = nullptr; // Transfer ownership
            }
            notification = std::move(initNotif);
            break;
        }
        case MessageType::PROGRESS_NOTIFICATION: {
            auto progNotif = std::make_unique<ProgressNotification>(progressToken_, progress_, total_);
            progNotif->setMessage(progressMessage_);
            progNotif->setDetails(progressDetails_);
            notification = std::move(progNotif);
            break;
        }
        case MessageType::CANCELLED_NOTIFICATION: {
            auto cancelNotif = std::make_unique<CancelledNotification>(requestId_, cancellationReason_);
            if (progressToken_.isValid()) {
                cancelNotif->setProgressToken(progressToken_);
            }
            if (errorCode_ != 0) {
                cancelNotif->setError(errorCode_, errorMessage_);
            }
            notification = std::move(cancelNotif);
            break;
        }
        default:
            // Handle special notifications
            if (method_ == "notifications/tools/list_changed") {
                auto toolsNotif = std::make_unique