// Request Message Classes for TinyMCP Protocol
// Implementation of JSON-RPC 2.0 compliant request messages for ESP32/ESP8266

#include "tinymcp_request.h"
#include <algorithm>
#include <cstring>

namespace tinymcp {

// Request implementation

Request::Request(MessageType type, const MessageId& id, const std::string& method)
    : Message(type, MessageCategory::REQUEST)
    , id_(id)
    , method_(method) {
}

bool Request::isValid() const {
    return id_.isValid() && !method_.empty() && 
           method_.length() <= MAX_METHOD_NAME_LENGTH;
}

int Request::serialize(std::string& jsonOut) const {
    JsonValue json = JsonValue::createObject();
    if (!json.isValid()) return -1;
    
    int result = doSerialize(json.get());
    if (result != 0) return result;
    
    jsonOut = JsonHelper::toString(json.get());
    return jsonOut.empty() ? -1 : 0;
}

int Request::deserialize(const std::string& jsonIn) {
    JsonValue json = JsonValue::parse(jsonIn);
    if (!json.isValid()) return TINYMCP_PARSE_ERROR;
    
    return doDeserialize(json.get());
}

int Request::doSerialize(cJSON* json) const {
    if (!json) return -1;
    
    // Add common JSON-RPC fields
    if (!addCommonFields(json)) return -1;
    
    // Add method
    if (!JsonHelper::setString(json, MSG_KEY_METHOD, method_)) return -1;
    
    // Add ID
    if (!id_.addToJson(json)) return -1;
    
    // Add parameters (implemented by derived classes)
    int result = serializeParams(json);
    if (result != 0) return result;
    
    return 0;
}

int Request::doDeserialize(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    // Validate common fields
    if (!validateCommonFields(json)) return TINYMCP_INVALID_REQUEST;
    
    // Validate and extract method
    if (!JsonHelper::isString(json, MSG_KEY_METHOD)) {
        return TINYMCP_INVALID_REQUEST;
    }
    method_ = JsonHelper::getString(json, MSG_KEY_METHOD);
    
    // Validate and extract ID
    if (!id_.setFromJson(json)) return TINYMCP_INVALID_REQUEST;
    
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

// InitializeRequest implementation

void InitializeRequest::setClientCapabilities(cJSON* capabilities) {
    if (clientCapabilities_) {
        cJSON_Delete(clientCapabilities_);
    }
    clientCapabilities_ = capabilities;
}

bool InitializeRequest::validateParams(const cJSON* params) const {
    if (!params) return false;
    
    // Protocol version is required
    if (!JsonHelper::isString(params, MSG_KEY_PROTOCOL_VERSION)) {
        return false;
    }
    
    // Client info is required
    if (!JsonHelper::isObject(params, MSG_KEY_CLIENT_INFO)) {
        return false;
    }
    
    cJSON* clientInfo = JsonHelper::getObject(params, MSG_KEY_CLIENT_INFO);
    if (!clientInfo) return false;
    
    // Validate client info structure
    if (!JsonHelper::isString(clientInfo, MSG_KEY_NAME) ||
        !JsonHelper::isString(clientInfo, MSG_KEY_VERSION)) {
        return false;
    }
    
    return true;
}

int InitializeRequest::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    // Add protocol version
    if (!JsonHelper::setString(params, MSG_KEY_PROTOCOL_VERSION, protocolVersion_)) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add client info
    cJSON* clientInfoJson = clientInfo_.toJson();
    if (!clientInfoJson) {
        cJSON_Delete(params);
        return -1;
    }
    
    if (!JsonHelper::setObject(params, MSG_KEY_CLIENT_INFO, clientInfoJson)) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add client capabilities if present
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

int InitializeRequest::deserializeParams(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_PARAMS;
    
    // Extract protocol version
    protocolVersion_ = JsonHelper::getString(json, MSG_KEY_PROTOCOL_VERSION);
    if (protocolVersion_.empty()) return TINYMCP_INVALID_PARAMS;
    
    // Extract client info
    cJSON* clientInfoJson = JsonHelper::getObject(json, MSG_KEY_CLIENT_INFO);
    if (!clientInfoJson || !clientInfo_.fromJson(clientInfoJson)) {
        return TINYMCP_INVALID_PARAMS;
    }
    
    // Extract client capabilities if present
    if (JsonHelper::hasField(json, MSG_KEY_CAPABILITIES)) {
        cJSON* capabilities = JsonHelper::getObject(json, MSG_KEY_CAPABILITIES);
        if (capabilities) {
            setClientCapabilities(JsonHelper::safeDuplicate(capabilities));
        }
    }
    
    return 0;
}

// ListToolsRequest implementation

bool ListToolsRequest::validateParams(const cJSON* params) const {
    if (!params) return true; // Parameters are optional for list tools
    
    // If cursor is present, it must be a string
    if (JsonHelper::hasField(params, MSG_KEY_CURSOR)) {
        if (!JsonHelper::isString(params, MSG_KEY_CURSOR)) return false;
    }
    
    // If maxResults is present, it must be a positive number
    if (JsonHelper::hasField(params, "maxResults")) {
        if (!JsonHelper::isNumber(params, "maxResults")) return false;
        int maxResults = JsonHelper::getInt(params, "maxResults");
        if (maxResults < 0) return false;
    }
    
    return true;
}

int ListToolsRequest::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    // Only add params if we have pagination parameters
    if (cursor_.empty() && maxResults_ == 0) {
        return 0; // No parameters needed
    }
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    if (!cursor_.empty()) {
        if (!JsonHelper::setString(params, MSG_KEY_CURSOR, cursor_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    if (maxResults_ > 0) {
        if (!JsonHelper::setInt(params, "maxResults", maxResults_)) {
            cJSON_Delete(params);
            return -1;
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int ListToolsRequest::deserializeParams(const cJSON* json) {
    if (!json) return 0; // No parameters is valid
    
    cursor_ = JsonHelper::getString(json, MSG_KEY_CURSOR);
    maxResults_ = JsonHelper::getInt(json, "maxResults", 0);
    
    return 0;
}

// ToolArgument implementation

cJSON* ToolArgument::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    JsonHelper::setString(json, MSG_KEY_NAME, name_);
    JsonHelper::setString(json, "value", value_);
    
    return json;
}

bool ToolArgument::fromJson(const cJSON* json) {
    if (!json) return false;
    
    name_ = JsonHelper::getString(json, MSG_KEY_NAME);
    value_ = JsonHelper::getString(json, "value");
    
    return !name_.empty();
}

// CallToolRequest implementation

CallToolRequest::CallToolRequest(const CallToolRequest& other)
    : Request(other.messageType_, other.id_, other.method_)
    , toolName_(other.toolName_)
    , arguments_(other.arguments_)
    , rawArguments_(nullptr) {
    
    if (other.rawArguments_) {
        rawArguments_ = JsonHelper::safeDuplicate(other.rawArguments_);
    }
}

CallToolRequest& CallToolRequest::operator=(const CallToolRequest& other) {
    if (this != &other) {
        Request::operator=(other);
        toolName_ = other.toolName_;
        arguments_ = other.arguments_;
        
        if (rawArguments_) {
            cJSON_Delete(rawArguments_);
            rawArguments_ = nullptr;
        }
        
        if (other.rawArguments_) {
            rawArguments_ = JsonHelper::safeDuplicate(other.rawArguments_);
        }
    }
    return *this;
}

void CallToolRequest::addArgument(const std::string& name, const std::string& value) {
    arguments_.emplace_back(name, value);
}

bool CallToolRequest::hasArgument(const std::string& name) const {
    return std::find_if(arguments_.begin(), arguments_.end(),
                       [&name](const ToolArgument& arg) {
                           return arg.getName() == name;
                       }) != arguments_.end();
}

std::string CallToolRequest::getArgumentValue(const std::string& name) const {
    auto it = std::find_if(arguments_.begin(), arguments_.end(),
                          [&name](const ToolArgument& arg) {
                              return arg.getName() == name;
                          });
    return (it != arguments_.end()) ? it->getValue() : "";
}

void CallToolRequest::setRawArguments(cJSON* args) {
    if (rawArguments_) {
        cJSON_Delete(rawArguments_);
    }
    rawArguments_ = args;
}

bool CallToolRequest::validateParams(const cJSON* params) const {
    if (!params) return false;
    
    // Tool name is required
    if (!JsonHelper::isString(params, MSG_KEY_NAME)) {
        return false;
    }
    
    std::string toolName = JsonHelper::getString(params, MSG_KEY_NAME);
    if (toolName.empty() || toolName.length() > MAX_TOOL_NAME_LENGTH) {
        return false;
    }
    
    // Arguments are optional but if present must be an object
    if (JsonHelper::hasField(params, MSG_KEY_ARGUMENTS)) {
        if (!JsonHelper::isObject(params, MSG_KEY_ARGUMENTS)) {
            return false;
        }
    }
    
    return true;
}

int CallToolRequest::serializeParams(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* params = cJSON_CreateObject();
    if (!params) return -1;
    
    // Add tool name
    if (!JsonHelper::setString(params, MSG_KEY_NAME, toolName_)) {
        cJSON_Delete(params);
        return -1;
    }
    
    // Add arguments
    if (rawArguments_) {
        // Use raw arguments if available
        cJSON* argsCopy = JsonHelper::safeDuplicate(rawArguments_);
        if (argsCopy) {
            JsonHelper::setObject(params, MSG_KEY_ARGUMENTS, argsCopy);
        }
    } else if (!arguments_.empty()) {
        // Convert ToolArgument vector to JSON object
        cJSON* argsObj = cJSON_CreateObject();
        if (argsObj) {
            for (const auto& arg : arguments_) {
                JsonHelper::setString(argsObj, arg.getName().c_str(), arg.getValue());
            }
            JsonHelper::setObject(params, MSG_KEY_ARGUMENTS, argsObj);
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_PARAMS, params)) {
        return -1;
    }
    
    return 0;
}

int CallToolRequest::deserializeParams(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_PARAMS;
    
    // Extract tool name
    toolName_ = JsonHelper::getString(json, MSG_KEY_NAME);
    if (toolName_.empty()) return TINYMCP_INVALID_PARAMS;
    
    // Extract arguments if present
    if (JsonHelper::hasField(json, MSG_KEY_ARGUMENTS)) {
        cJSON* args = JsonHelper::getObject(json, MSG_KEY_ARGUMENTS);
        if (args) {
            // Store raw arguments for complex structures
            setRawArguments(JsonHelper::safeDuplicate(args));
            
            // Also convert to ToolArgument vector for simple cases
            arguments_.clear();
            cJSON* child = args->child;
            while (child) {
                if (child->string && cJSON_IsString(child)) {
                    const char* value = cJSON_GetStringValue(child);
                    if (value) {
                        arguments_.emplace_back(child->string, value);
                    }
                }
                child = child->next;
            }
        }
    }
    
    return 0;
}

// RequestFactory implementation

std::unique_ptr<Request> RequestFactory::createFromJson(const std::string& jsonStr) {
    JsonValue json = JsonValue::parse(jsonStr);
    if (!json.isValid()) return nullptr;
    
    return createFromJson(json.get());
}

std::unique_ptr<Request> RequestFactory::createFromJson(const cJSON* json) {
    if (!json || !JsonHelper::validateRequest(json)) return nullptr;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    MessageType type = getRequestType(method);
    
    if (type == MessageType::UNKNOWN) return nullptr;
    
    // Extract ID
    MessageId id;
    if (!id.setFromJson(json)) return nullptr;
    
    std::unique_ptr<Request> request;
    
    switch (type) {
        case MessageType::INITIALIZE_REQUEST: {
            auto initReq = std::make_unique<InitializeRequest>(id);
            if (initReq->deserialize(JsonHelper::toString(json)) == 0) {
                request = std::move(initReq);
            }
            break;
        }
        case MessageType::LIST_TOOLS_REQUEST: {
            auto listReq = std::make_unique<ListToolsRequest>(id);
            if (listReq->deserialize(JsonHelper::toString(json)) == 0) {
                request = std::move(listReq);
            }
            break;
        }
        case MessageType::CALL_TOOL_REQUEST: {
            auto callReq = std::make_unique<CallToolRequest>(id, "");
            if (callReq->deserialize(JsonHelper::toString(json)) == 0) {
                request = std::move(callReq);
            }
            break;
        }
        case MessageType::PING_REQUEST: {
            auto pingReq = std::make_unique<PingRequest>(id);
            if (pingReq->deserialize(JsonHelper::toString(json)) == 0) {
                request = std::move(pingReq);
            }
            break;
        }
        default:
            break;
    }
    
    return request;
}

std::unique_ptr<InitializeRequest> RequestFactory::createInitializeRequest(
    const MessageId& id,
    const std::string& protocolVersion,
    const ClientInfo& clientInfo) {
    
    auto request = std::make_unique<InitializeRequest>(id);
    request->setProtocolVersion(protocolVersion);
    request->setClientInfo(clientInfo);
    return request;
}

std::unique_ptr<ListToolsRequest> RequestFactory::createListToolsRequest(
    const MessageId& id,
    const std::string& cursor,
    int maxResults) {
    
    auto request = std::make_unique<ListToolsRequest>(id);
    if (!cursor.empty()) request->setCursor(cursor);
    if (maxResults > 0) request->setMaxResults(maxResults);
    return request;
}

std::unique_ptr<CallToolRequest> RequestFactory::createCallToolRequest(
    const MessageId& id,
    const std::string& toolName,
    const std::vector<ToolArgument>& arguments) {
    
    auto request = std::make_unique<CallToolRequest>(id, toolName);
    request->setArguments(arguments);
    return request;
}

std::unique_ptr<PingRequest> RequestFactory::createPingRequest(const MessageId& id) {
    return std::make_unique<PingRequest>(id);
}

MessageType RequestFactory::getRequestType(const std::string& method) {
    if (method == METHOD_INITIALIZE) return MessageType::INITIALIZE_REQUEST;
    if (method == METHOD_TOOLS_LIST) return MessageType::LIST_TOOLS_REQUEST;
    if (method == METHOD_TOOLS_CALL) return MessageType::CALL_TOOL_REQUEST;
    if (method == METHOD_PING) return MessageType::PING_REQUEST;
    return MessageType::UNKNOWN;
}

// RequestValidator implementation

bool RequestValidator::validateInitializeRequest(const cJSON* json) {
    if (!JsonHelper::validateRequest(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    if (method != METHOD_INITIALIZE) return false;
    
    cJSON* params = JsonHelper::getObject(json, MSG_KEY_PARAMS);
    if (!params) return false;
    
    // Validate protocol version
    std::string protocolVersion = JsonHelper::getString(params, MSG_KEY_PROTOCOL_VERSION);
    if (!validateProtocolVersion(protocolVersion)) return false;
    
    // Validate client info
    cJSON* clientInfo = JsonHelper::getObject(params, MSG_KEY_CLIENT_INFO);
    if (!validateClientInfo(clientInfo)) return false;
    
    return true;
}

bool RequestValidator::validateListToolsRequest(const cJSON* json) {
    if (!JsonHelper::validateRequest(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    if (method != METHOD_TOOLS_LIST) return false;
    
    // Parameters are optional for list tools
    return true;
}

bool RequestValidator::validateCallToolRequest(const cJSON* json) {
    if (!JsonHelper::validateRequest(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    if (method != METHOD_TOOLS_CALL) return false;
    
    cJSON* params = JsonHelper::getObject(json, MSG_KEY_PARAMS);
    if (!params) return false;
    
    // Validate tool name
    std::string toolName = JsonHelper::getString(params, MSG_KEY_NAME);
    if (!validateToolName(toolName)) return false;
    
    // Validate arguments if present
    if (JsonHelper::hasField(params, MSG_KEY_ARGUMENTS)) {
        cJSON* arguments = JsonHelper::getObject(params, MSG_KEY_ARGUMENTS);
        if (!validateToolArguments(arguments)) return false;
    }
    
    return true;
}

bool RequestValidator::validatePingRequest(const cJSON* json) {
    if (!JsonHelper::validateRequest(json)) return false;
    
    std::string method = JsonHelper::getString(json, MSG_KEY_METHOD);
    return method == METHOD_PING;
}

bool RequestValidator::validateProtocolVersion(const std::string& version) {
    return !version.empty() && version.length() <= 32;
}

bool RequestValidator::validateClientInfo(const cJSON* clientInfo) {
    if (!clientInfo) return false;
    
    std::string name = JsonHelper::getString(clientInfo, MSG_KEY_NAME);
    std::string version = JsonHelper::getString(clientInfo, MSG_KEY_VERSION);
    
    return !name.empty() && !version.empty() && 
           name.length() <= 64 && version.length() <= 32;
}

bool RequestValidator::validateToolName(const std::string& toolName) {
    if (toolName.empty() || toolName.length() > MAX_TOOL_NAME_LENGTH) {
        return false;
    }
    
    // Tool name should contain only valid characters
    for (char c : toolName) {
        if (!isValidToolNameChar(c)) return false;
    }
    
    return true;
}

bool RequestValidator::validateToolArguments(const cJSON* arguments) {
    if (!arguments || !cJSON_IsObject(arguments)) return false;
    
    cJSON* child = arguments->child;
    while (child) {
        if (!child->string || !isValidParameterName(child->string)) {
            return false;
        }
        child = child->next;
    }
    
    return true;
}

bool RequestValidator::isValidToolNameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

bool RequestValidator::isValidParameterName(const std::string& name) {
    if (name.empty() || name.length() > 64) return false;
    
    for (char c : name) {
        if (!isValidToolNameChar(c)) return false;
    }
    
    return true;
}

// RequestBuilder implementation

RequestBuilder& RequestBuilder::initializeRequest(const MessageId& id) {
    requestType_ = MessageType::INITIALIZE_REQUEST;
    id_ = id;
    method_ = METHOD_INITIALIZE;
    return *this;
}

RequestBuilder& RequestBuilder::withProtocolVersion(const std::string& version) {
    protocolVersion_ = version;
    return *this;
}

RequestBuilder& RequestBuilder::withClientInfo(const std::string& name, const std::string& version) {
    clientInfo_ = ClientInfo(name, version);
    return *this;
}

RequestBuilder& RequestBuilder::withClientCapabilities(cJSON* capabilities) {
    if (clientCapabilities_) {
        cJSON_Delete(clientCapabilities_);
    }
    clientCapabilities_ = capabilities;
    return *this;
}

RequestBuilder& RequestBuilder::listToolsRequest(const MessageId& id) {
    requestType_ = MessageType::LIST_TOOLS_REQUEST;
    id_ = id;
    method_ = METHOD_TOOLS_LIST;
    return *this;
}

RequestBuilder& RequestBuilder::withCursor(const std::string& cursor) {
    cursor_ = cursor;
    return *this;
}

RequestBuilder& RequestBuilder::withMaxResults(int maxResults) {
    maxResults_ = maxResults;
    return *this;
}

RequestBuilder& RequestBuilder::callToolRequest(const MessageId& id, const std::string& toolName) {
    requestType_ = MessageType::CALL_TOOL_REQUEST;
    id_ = id;
    method_ = METHOD_TOOLS_CALL;
    toolName_ = toolName;
    return *this;
}

RequestBuilder& RequestBuilder::withArgument(const std::string& name, const std::string& value) {
    arguments_.emplace_back(name, value);
    return *this;
}

RequestBuilder& RequestBuilder::withRawArguments(cJSON* arguments) {
    if (rawArguments_) {
        cJSON_Delete(rawArguments_);
    }
    rawArguments_ = arguments;
    return *this;
}

RequestBuilder& RequestBuilder::pingRequest(const MessageId& id) {
    requestType_ = MessageType::PING_REQUEST;
    id_ = id;
    method_ = METHOD_PING;
    return *this;
}

RequestBuilder& RequestBuilder::withProgressToken(const std::string& token) {
    progressToken_ = token;
    return *this;
}

std::unique_ptr<Request> RequestBuilder::build() {
    std::unique_ptr<Request> request;
    
    switch (requestType_) {
        case MessageType::INITIALIZE_REQUEST: {
            auto initReq = std::make_unique<InitializeRequest>(id_);
            initReq->setProtocolVersion(protocolVersion_);
            initReq->setClientInfo(clientInfo_);
            if (clientCapabilities_) {
                initReq->setClientCapabilities(clientCapabilities_);
                clientCapabilities_ = nullptr; // Transfer ownership
            }
            request = std::move(initReq);
            break;
        }
        case MessageType::LIST_TOOLS_REQUEST: {
            auto listReq = std::make_unique<ListToolsRequest>(id_);
            listReq->setCursor(cursor_);
            listReq->setMaxResults(maxResults_);
            request = std::move(listReq);
            break;
        }
        case MessageType::CALL_TOOL_REQUEST: {
            auto callReq = std::make_unique<CallToolRequest>(id_, toolName_);
            callReq->setArguments(arguments_);
            if (rawArguments_) {
                callReq->setRawArguments(rawArguments_);
                rawArguments_ = nullptr; // Transfer ownership
            }
            request = std::move(callReq);
            break;
        }
        case MessageType::PING_REQUEST: {
            request = std::make_unique<PingRequest>(id_);
            break;
        }
        default:
            return nullptr;
    }
    
    if (request && !progressToken_.empty()) {
        request->setProgressToken(progressToken_);
    }
    
    return request;
}

void RequestBuilder::reset() {
    requestType_ = MessageType::UNKNOWN;
    id_ = MessageId();
    method_.clear();
    protocolVersion_.clear();
    clientInfo_ = ClientInfo("", "");
    if (clientCapabilities_) {
        cJSON_Delete(clientCapabilities_);
        clientCapabilities_ = nullptr;
    }
    cursor_.clear();
    maxResults_ = 0;
    toolName_.clear();
    arguments_.clear();
    if (rawArguments_) {
        cJSON_Delete(rawArguments_);
        rawArguments_ = nullptr;
    }
    progressToken_.clear();
}

} // namespace tinymcp