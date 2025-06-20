// Base Message Classes for TinyMCP Protocol
// Implementation of foundational message architecture for ESP32/ESP8266

#include "tinymcp_message.h"
#include <chrono>
#include <cstring>
#include <algorithm>

namespace tinymcp {

// MessageId implementation

bool MessageId::setFromJson(const cJSON* json) {
    if (!json) return false;
    
    cJSON* id = cJSON_GetObjectItem(json, MSG_KEY_ID);
    if (!id) return false;
    
    if (cJSON_IsString(id)) {
        const char* str = cJSON_GetStringValue(id);
        if (str) {
            type_ = DataType::STRING;
            stringId_ = str;
            intId_ = 0;
            return true;
        }
    } else if (cJSON_IsNumber(id)) {
        type_ = DataType::INTEGER;
        intId_ = static_cast<int>(cJSON_GetNumberValue(id));
        stringId_.clear();
        return true;
    }
    
    return false;
}

bool MessageId::addToJson(cJSON* json) const {
    if (!json || type_ == DataType::UNKNOWN) return false;
    
    if (type_ == DataType::STRING) {
        return JsonHelper::setString(json, MSG_KEY_ID, stringId_);
    } else if (type_ == DataType::INTEGER) {
        return JsonHelper::setInt(json, MSG_KEY_ID, intId_);
    }
    
    return false;
}

bool MessageId::operator==(const MessageId& other) const {
    if (type_ != other.type_) return false;
    
    if (type_ == DataType::STRING) {
        return stringId_ == other.stringId_;
    } else if (type_ == DataType::INTEGER) {
        return intId_ == other.intId_;
    }
    
    return type_ == DataType::UNKNOWN;
}

// Message implementation

Message::Message(MessageType type, MessageCategory category)
    : messageType_(type)
    , messageCategory_(category)
    , timestamp_(generateTimestamp()) {
}

bool Message::validateJsonRpc(const cJSON* json) const {
    return JsonHelper::validateJsonRpc(json);
}

std::unique_ptr<Message> Message::createFromJson(const std::string& jsonStr) {
    JsonValue json = JsonValue::parse(jsonStr);
    if (!json.isValid()) return nullptr;
    
    MessageType type = detectMessageType(json.get());
    MessageCategory category = detectMessageCategory(json.get());
    
    if (type == MessageType::UNKNOWN || category == MessageCategory::UNKNOWN) {
        return nullptr;
    }
    
    // Factory creation would go here - for now return nullptr
    // This would be implemented by the request/response/notification factories
    return nullptr;
}

MessageType Message::detectMessageType(const cJSON* json) {
    if (!json) return MessageType::UNKNOWN;
    
    bool hasMethod = JsonHelper::hasField(json, MSG_KEY_METHOD);
    bool hasId = JsonHelper::hasField(json, MSG_KEY_ID);
    bool hasResult = JsonHelper::hasField(json, MSG_KEY_RESULT);
    bool hasError = JsonHelper::hasField(json, MSG_KEY_ERROR);
    
    if (hasMethod && hasId) {
        // Request - determine type by method
        std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
        if (method == METHOD_INITIALIZE) return MessageType::INITIALIZE_REQUEST;
        if (method == METHOD_TOOLS_LIST) return MessageType::LIST_TOOLS_REQUEST;
        if (method == METHOD_TOOLS_CALL) return MessageType::CALL_TOOL_REQUEST;
        if (method == METHOD_PING) return MessageType::PING_REQUEST;
        return MessageType::UNKNOWN;
    } else if (hasMethod && !hasId) {
        // Notification - determine type by method
        std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
        if (method == METHOD_INITIALIZED) return MessageType::INITIALIZED_NOTIFICATION;
        if (method == METHOD_PROGRESS) return MessageType::PROGRESS_NOTIFICATION;
        if (method == METHOD_CANCELLED) return MessageType::CANCELLED_NOTIFICATION;
        return MessageType::UNKNOWN;
    } else if (!hasMethod && hasId && (hasResult || hasError)) {
        // Response - would need more context to determine exact type
        if (hasError) return MessageType::ERROR_RESPONSE;
        // For success responses, we'd need to look at the request context
        return MessageType::UNKNOWN;
    }
    
    return MessageType::UNKNOWN;
}

MessageCategory Message::detectMessageCategory(const cJSON* json) {
    if (!json) return MessageCategory::UNKNOWN;
    
    bool hasMethod = JsonHelper::hasField(json, MSG_KEY_METHOD);
    bool hasId = JsonHelper::hasField(json, MSG_KEY_ID);
    bool hasResult = JsonHelper::hasField(json, MSG_KEY_RESULT);
    bool hasError = JsonHelper::hasField(json, MSG_KEY_ERROR);
    
    if (hasMethod && hasId) {
        return MessageCategory::REQUEST;
    } else if (hasMethod && !hasId) {
        return MessageCategory::NOTIFICATION;
    } else if (!hasMethod && hasId && (hasResult || hasError)) {
        return MessageCategory::RESPONSE;
    }
    
    return MessageCategory::UNKNOWN;
}

bool Message::exceedsMaxSize() const {
    std::string jsonStr;
    if (serialize(jsonStr) != 0) return true;
    return jsonStr.length() > MAX_MESSAGE_SIZE;
}

size_t Message::estimateSize() const {
    std::string jsonStr;
    if (serialize(jsonStr) != 0) return 0;
    return jsonStr.length();
}

bool Message::validateCommonFields(const cJSON* json) const {
    if (!validateJsonRpc(json)) return false;
    
    // Check for progress token if present
    if (JsonHelper::hasField(json, MSG_KEY_PROGRESS_TOKEN)) {
        if (!JsonHelper::isString(json, MSG_KEY_PROGRESS_TOKEN)) {
            return false;
        }
    }
    
    return true;
}

bool Message::addCommonFields(cJSON* json) const {
    if (!json) return false;
    
    // Add JSON-RPC version
    if (!JsonHelper::setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION)) {
        return false;
    }
    
    // Add progress token if present
    if (!progressToken_.empty()) {
        if (!JsonHelper::setString(json, MSG_KEY_PROGRESS_TOKEN, progressToken_)) {
            return false;
        }
    }
    
    return true;
}

uint64_t Message::generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(duration.count());
}

// ProgressToken implementation

ProgressToken ProgressToken::generate() {
    // Simple timestamp-based token generation
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return ProgressToken("prog_" + std::to_string(duration.count()));
}

// Content implementation

cJSON* Content::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    const char* typeStr = (type_ == ContentType::TEXT) ? "text" : 
                         (type_ == ContentType::IMAGE) ? "image" : "resource";
    
    JsonHelper::setString(json, MSG_KEY_TYPE, typeStr);
    JsonHelper::setString(json, MSG_KEY_TEXT, data_);
    
    return json;
}

bool Content::fromJson(const cJSON* json) {
    if (!json) return false;
    
    std::string typeStr = JsonHelper::getString(json, MSG_KEY_TYPE);
    if (typeStr == "text") {
        type_ = ContentType::TEXT;
    } else if (typeStr == "image") {
        type_ = ContentType::IMAGE;
    } else if (typeStr == "resource") {
        type_ = ContentType::RESOURCE;
    } else {
        return false;
    }
    
    data_ = JsonHelper::getString(json, MSG_KEY_TEXT);
    return !data_.empty();
}

Content Content::createText(const std::string& text) {
    return Content(ContentType::TEXT, text);
}

Content Content::createError(const std::string& error) {
    return Content(ContentType::TEXT, error);
}

// Error implementation

cJSON* Error::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    JsonHelper::setInt(json, MSG_KEY_CODE, code_);
    JsonHelper::setString(json, MSG_KEY_MESSAGE, message_);
    
    if (!data_.empty()) {
        JsonHelper::setString(json, MSG_KEY_DATA, data_);
    }
    
    return json;
}

bool Error::fromJson(const cJSON* json) {
    if (!json) return false;
    
    code_ = JsonHelper::getInt(json, MSG_KEY_CODE);
    message_ = JsonHelper::getString(json, MSG_KEY_MESSAGE);
    data_ = JsonHelper::getString(json, MSG_KEY_DATA);
    
    return code_ != 0 && !message_.empty();
}

Error Error::parseError(const std::string& details) {
    std::string msg = ERROR_MSG_PARSE_ERROR;
    if (!details.empty()) msg += ": " + details;
    return Error(TINYMCP_PARSE_ERROR, msg, details);
}

Error Error::invalidRequest(const std::string& details) {
    std::string msg = ERROR_MSG_INVALID_REQUEST;
    if (!details.empty()) msg += ": " + details;
    return Error(TINYMCP_INVALID_REQUEST, msg, details);
}

Error Error::methodNotFound(const std::string& method) {
    std::string msg = ERROR_MSG_METHOD_NOT_FOUND;
    if (!method.empty()) msg += ": " + method;
    return Error(TINYMCP_METHOD_NOT_FOUND, msg, method);
}

Error Error::invalidParams(const std::string& details) {
    std::string msg = ERROR_MSG_INVALID_PARAMS;
    if (!details.empty()) msg += ": " + details;
    return Error(TINYMCP_INVALID_PARAMS, msg, details);
}

Error Error::internalError(const std::string& details) {
    std::string msg = ERROR_MSG_INTERNAL_ERROR;
    if (!details.empty()) msg += ": " + details;
    return Error(TINYMCP_INTERNAL_ERROR, msg, details);
}

Error Error::notInitialized() {
    return Error(TINYMCP_NOT_INITIALIZED, ERROR_MSG_NOT_INITIALIZED);
}

Error Error::toolError(const std::string& details) {
    std::string msg = ERROR_MSG_TOOL_ERROR;
    if (!details.empty()) msg += ": " + details;
    return Error(TINYMCP_TOOL_ERROR, msg, details);
}

// ServerInfo implementation

cJSON* ServerInfo::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    JsonHelper::setString(json, MSG_KEY_NAME, name_);
    JsonHelper::setString(json, MSG_KEY_VERSION, version_);
    
    return json;
}

bool ServerInfo::fromJson(const cJSON* json) {
    if (!json) return false;
    
    name_ = JsonHelper::getString(json, MSG_KEY_NAME);
    version_ = JsonHelper::getString(json, MSG_KEY_VERSION);
    
    return !name_.empty() && !version_.empty();
}

// ClientInfo implementation

cJSON* ClientInfo::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    JsonHelper::setString(json, MSG_KEY_NAME, name_);
    JsonHelper::setString(json, MSG_KEY_VERSION, version_);
    
    return json;
}

bool ClientInfo::fromJson(const cJSON* json) {
    if (!json) return false;
    
    name_ = JsonHelper::getString(json, MSG_KEY_NAME);
    version_ = JsonHelper::getString(json, MSG_KEY_VERSION);
    
    return !name_.empty() && !version_.empty();
}

// ServerCapabilities implementation

cJSON* ServerCapabilities::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    // Tools capabilities
    cJSON* tools = cJSON_CreateObject();
    if (tools) {
        JsonHelper::setBool(tools, MSG_KEY_LISTCHANGED, toolsListChanged_);
        JsonHelper::setObject(json, MSG_KEY_TOOLS, tools);
    }
    
    // Add other capabilities as needed
    if (progressNotifications_) {
        cJSON* logging = cJSON_CreateObject();
        if (logging) {
            JsonHelper::setObject(json, "logging", logging);
        }
    }
    
    return json;
}

bool ServerCapabilities::fromJson(const cJSON* json) {
    if (!json) return false;
    
    // Parse tools capabilities
    cJSON* tools = JsonHelper::getObject(json, MSG_KEY_TOOLS);
    if (tools) {
        toolsListChanged_ = JsonHelper::getBool(tools, MSG_KEY_LISTCHANGED, false);
    }
    
    // Parse other capabilities
    progressNotifications_ = JsonHelper::hasField(json, "logging");
    
    return true;
}

// MessageValidator implementation

MessageValidationResult MessageValidator::validate(const std::string& jsonStr) {
    JsonValue json = JsonValue::parse(jsonStr);
    if (!json.isValid()) {
        return MessageValidationResult(TINYMCP_PARSE_ERROR, "Invalid JSON");
    }
    
    return validate(json.get());
}

MessageValidationResult MessageValidator::validate(const cJSON* json) {
    if (!json) {
        return MessageValidationResult(TINYMCP_PARSE_ERROR, "Null JSON");
    }
    
    if (!isValidJsonRpc(json)) {
        return MessageValidationResult(TINYMCP_INVALID_REQUEST, "Invalid JSON-RPC version");
    }
    
    MessageCategory category = Message::detectMessageCategory(json);
    MessageType type = Message::detectMessageType(json);
    
    if (category == MessageCategory::UNKNOWN) {
        return MessageValidationResult(TINYMCP_INVALID_REQUEST, "Unknown message category");
    }
    
    if (type == MessageType::UNKNOWN) {
        return MessageValidationResult(TINYMCP_METHOD_NOT_FOUND, "Unknown message type");
    }
    
    if (!validateMessageStructure(json, category)) {
        return MessageValidationResult(TINYMCP_INVALID_REQUEST, "Invalid message structure");
    }
    
    MessageValidationResult result(true);
    result.detectedType = type;
    result.detectedCategory = category;
    return result;
}

bool MessageValidator::isValidJsonRpc(const cJSON* json) {
    return JsonHelper::validateJsonRpc(json);
}

bool MessageValidator::isValidMethod(const std::string& method) {
    return !method.empty() && method.length() <= MAX_METHOD_NAME_LENGTH;
}

bool MessageValidator::isValidId(const cJSON* idJson) {
    return idJson && (cJSON_IsString(idJson) || cJSON_IsNumber(idJson));
}

MessageType MessageValidator::getMessageTypeFromMethod(const std::string& method) {
    if (method == METHOD_INITIALIZE) return MessageType::INITIALIZE_REQUEST;
    if (method == METHOD_TOOLS_LIST) return MessageType::LIST_TOOLS_REQUEST;
    if (method == METHOD_TOOLS_CALL) return MessageType::CALL_TOOL_REQUEST;
    if (method == METHOD_PING) return MessageType::PING_REQUEST;
    if (method == METHOD_INITIALIZED) return MessageType::INITIALIZED_NOTIFICATION;
    if (method == METHOD_PROGRESS) return MessageType::PROGRESS_NOTIFICATION;
    if (method == METHOD_CANCELLED) return MessageType::CANCELLED_NOTIFICATION;
    return MessageType::UNKNOWN;
}

bool MessageValidator::validateMessageStructure(const cJSON* json, MessageCategory category) {
    switch (category) {
        case MessageCategory::REQUEST:
            return JsonHelper::validateRequest(json);
        case MessageCategory::RESPONSE:
            return JsonHelper::validateResponse(json);
        case MessageCategory::NOTIFICATION:
            return JsonHelper::validateNotification(json);
        default:
            return false;
    }
}

} // namespace tinymcp