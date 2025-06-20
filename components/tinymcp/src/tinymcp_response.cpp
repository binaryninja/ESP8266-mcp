// Response Message Classes for TinyMCP Protocol
// Implementation of JSON-RPC 2.0 compliant response messages for ESP32/ESP8266

#include "tinymcp_response.h"
#include <algorithm>
#include <cstring>

namespace tinymcp {

// Response implementation

Response::Response(MessageType type, const MessageId& id)
    : Message(type, MessageCategory::RESPONSE)
    , id_(id)
    , isError_(false) {
}

bool Response::isValid() const {
    return id_.isValid();
}

int Response::serialize(std::string& jsonOut) const {
    JsonValue json = JsonValue::createObject();
    if (!json.isValid()) return -1;
    
    int result = doSerialize(json.get());
    if (result != 0) return result;
    
    jsonOut = JsonHelper::toString(json.get());
    return jsonOut.empty() ? -1 : 0;
}

int Response::deserialize(const std::string& jsonIn) {
    JsonValue json = JsonValue::parse(jsonIn);
    if (!json.isValid()) return TINYMCP_PARSE_ERROR;
    
    return doDeserialize(json.get());
}

int Response::doSerialize(cJSON* json) const {
    if (!json) return -1;
    
    // Add common JSON-RPC fields
    if (!addCommonFields(json)) return -1;
    
    // Add ID
    if (!id_.addToJson(json)) return -1;
    
    // Add result or error (implemented by derived classes)
    int result = serializeResult(json);
    if (result != 0) return result;
    
    return 0;
}

int Response::doDeserialize(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    // Validate common fields
    if (!validateCommonFields(json)) return TINYMCP_INVALID_REQUEST;
    
    // Validate and extract ID
    if (!id_.setFromJson(json)) return TINYMCP_INVALID_REQUEST;
    
    // Check if this is an error response
    isError_ = JsonHelper::hasField(json, MSG_KEY_ERROR);
    
    // Deserialize result/error (implemented by derived classes)
    int result = deserializeResult(json);
    if (result != 0) return result;
    
    return 0;
}

// ErrorResponse implementation

int ErrorResponse::serializeResult(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* errorObj = error_.toJson();
    if (!errorObj) return -1;
    
    if (!JsonHelper::setObject(json, MSG_KEY_ERROR, errorObj)) {
        return -1;
    }
    
    return 0;
}

int ErrorResponse::deserializeResult(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    cJSON* errorObj = JsonHelper::getObject(json, MSG_KEY_ERROR);
    if (!errorObj) return TINYMCP_INVALID_REQUEST;
    
    if (!error_.fromJson(errorObj)) {
        return TINYMCP_INVALID_REQUEST;
    }
    
    return 0;
}

// InitializeResponse implementation

int InitializeResponse::serializeResult(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* result = cJSON_CreateObject();
    if (!result) return -1;
    
    // Add protocol version
    if (!JsonHelper::setString(result, MSG_KEY_PROTOCOL_VERSION, protocolVersion_)) {
        cJSON_Delete(result);
        return -1;
    }
    
    // Add server info
    cJSON* serverInfoJson = serverInfo_.toJson();
    if (!serverInfoJson) {
        cJSON_Delete(result);
        return -1;
    }
    JsonHelper::setObject(result, MSG_KEY_SERVER_INFO, serverInfoJson);
    
    // Add capabilities
    cJSON* capabilitiesJson = capabilities_.toJson();
    if (!capabilitiesJson) {
        cJSON_Delete(result);
        return -1;
    }
    JsonHelper::setObject(result, MSG_KEY_CAPABILITIES, capabilitiesJson);
    
    // Add instructions if present
    if (!instructions_.empty()) {
        JsonHelper::setString(result, "instructions", instructions_);
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_RESULT, result)) {
        return -1;
    }
    
    return 0;
}

int InitializeResponse::deserializeResult(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    cJSON* result = JsonHelper::getObject(json, MSG_KEY_RESULT);
    if (!result) return TINYMCP_INVALID_REQUEST;
    
    // Extract protocol version
    protocolVersion_ = JsonHelper::getString(result, MSG_KEY_PROTOCOL_VERSION);
    
    // Extract server info
    cJSON* serverInfoJson = JsonHelper::getObject(result, MSG_KEY_SERVER_INFO);
    if (serverInfoJson) {
        serverInfo_.fromJson(serverInfoJson);
    }
    
    // Extract capabilities
    cJSON* capabilitiesJson = JsonHelper::getObject(result, MSG_KEY_CAPABILITIES);
    if (capabilitiesJson) {
        capabilities_.fromJson(capabilitiesJson);
    }
    
    // Extract instructions if present
    instructions_ = JsonHelper::getString(result, "instructions");
    
    return 0;
}

// Tool implementation

Tool::Tool(const Tool& other)
    : name_(other.name_)
    , description_(other.description_)
    , inputSchema_(nullptr) {
    if (other.inputSchema_) {
        copyInputSchema(other.inputSchema_);
    }
}

Tool& Tool::operator=(const Tool& other) {
    if (this != &other) {
        name_ = other.name_;
        description_ = other.description_;
        
        if (inputSchema_) {
            cJSON_Delete(inputSchema_);
            inputSchema_ = nullptr;
        }
        
        if (other.inputSchema_) {
            copyInputSchema(other.inputSchema_);
        }
    }
    return *this;
}

Tool::Tool(Tool&& other) noexcept
    : name_(std::move(other.name_))
    , description_(std::move(other.description_))
    , inputSchema_(other.inputSchema_) {
    other.inputSchema_ = nullptr;
}

Tool& Tool::operator=(Tool&& other) noexcept {
    if (this != &other) {
        name_ = std::move(other.name_);
        description_ = std::move(other.description_);
        
        if (inputSchema_) {
            cJSON_Delete(inputSchema_);
        }
        
        inputSchema_ = other.inputSchema_;
        other.inputSchema_ = nullptr;
    }
    return *this;
}

void Tool::setInputSchema(cJSON* schema) {
    if (inputSchema_) {
        cJSON_Delete(inputSchema_);
    }
    inputSchema_ = schema;
}

cJSON* Tool::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    JsonHelper::setString(json, MSG_KEY_NAME, name_);
    JsonHelper::setString(json, MSG_KEY_DESCRIPTION, description_);
    
    if (inputSchema_) {
        cJSON* schemaCopy = JsonHelper::safeDuplicate(inputSchema_);
        if (schemaCopy) {
            JsonHelper::setObject(json, MSG_KEY_INPUT_SCHEMA, schemaCopy);
        }
    }
    
    return json;
}

bool Tool::fromJson(const cJSON* json) {
    if (!json) return false;
    
    name_ = JsonHelper::getString(json, MSG_KEY_NAME);
    description_ = JsonHelper::getString(json, MSG_KEY_DESCRIPTION);
    
    if (JsonHelper::hasField(json, MSG_KEY_INPUT_SCHEMA)) {
        cJSON* schema = JsonHelper::getObject(json, MSG_KEY_INPUT_SCHEMA);
        if (schema) {
            setInputSchema(JsonHelper::safeDuplicate(schema));
        }
    }
    
    return !name_.empty() && !description_.empty();
}

void Tool::copyInputSchema(const cJSON* schema) {
    if (schema) {
        inputSchema_ = JsonHelper::safeDuplicate(schema);
    }
}

// ListToolsResponse implementation

bool ListToolsResponse::hasTool(const std::string& name) const {
    return std::find_if(tools_.begin(), tools_.end(),
                       [&name](const Tool& tool) {
                           return tool.getName() == name;
                       }) != tools_.end();
}

const Tool* ListToolsResponse::getTool(const std::string& name) const {
    auto it = std::find_if(tools_.begin(), tools_.end(),
                          [&name](const Tool& tool) {
                              return tool.getName() == name;
                          });
    return (it != tools_.end()) ? &(*it) : nullptr;
}

int ListToolsResponse::serializeResult(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* result = cJSON_CreateObject();
    if (!result) return -1;
    
    // Add tools array
    cJSON* toolsArray = cJSON_CreateArray();
    if (!toolsArray) {
        cJSON_Delete(result);
        return -1;
    }
    
    for (const auto& tool : tools_) {
        cJSON* toolJson = tool.toJson();
        if (toolJson) {
            JsonHelper::addToArray(toolsArray, toolJson);
        }
    }
    
    JsonHelper::setArray(result, MSG_KEY_TOOLS, toolsArray);
    
    // Add next cursor if present
    if (!nextCursor_.empty()) {
        JsonHelper::setString(result, MSG_KEY_NEXT_CURSOR, nextCursor_);
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_RESULT, result)) {
        return -1;
    }
    
    return 0;
}

int ListToolsResponse::deserializeResult(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    cJSON* result = JsonHelper::getObject(json, MSG_KEY_RESULT);
    if (!result) return TINYMCP_INVALID_REQUEST;
    
    // Extract tools array
    cJSON* toolsArray = JsonHelper::getArray(result, MSG_KEY_TOOLS);
    if (toolsArray) {
        tools_.clear();
        int arraySize = JsonHelper::getArraySize(toolsArray);
        for (int i = 0; i < arraySize; i++) {
            cJSON* toolJson = JsonHelper::getArrayItem(toolsArray, i);
            if (toolJson) {
                Tool tool("", "");
                if (tool.fromJson(toolJson)) {
                    tools_.push_back(std::move(tool));
                }
            }
        }
    }
    
    // Extract next cursor if present
    nextCursor_ = JsonHelper::getString(result, MSG_KEY_NEXT_CURSOR);
    
    return 0;
}

// ToolContent implementation

cJSON* ToolContent::toJson() const {
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;
    
    const char* typeStr = (type_ == ContentType::TEXT) ? "text" : 
                         (type_ == ContentType::IMAGE) ? "image" : "resource";
    
    JsonHelper::setString(json, MSG_KEY_TYPE, typeStr);
    JsonHelper::setString(json, MSG_KEY_TEXT, text_);
    
    if (!mimeType_.empty()) {
        JsonHelper::setString(json, MSG_KEY_MIMETYPE, mimeType_);
    }
    
    return json;
}

bool ToolContent::fromJson(const cJSON* json) {
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
    
    text_ = JsonHelper::getString(json, MSG_KEY_TEXT);
    mimeType_ = JsonHelper::getString(json, MSG_KEY_MIMETYPE, "text/plain");
    
    return !text_.empty();
}

ToolContent ToolContent::createText(const std::string& text) {
    return ToolContent(ContentType::TEXT, text);
}

ToolContent ToolContent::createError(const std::string& error) {
    return ToolContent(ContentType::TEXT, error, "text/plain");
}

ToolContent ToolContent::createJson(const std::string& json) {
    return ToolContent(ContentType::TEXT, json, "application/json");
}

// CallToolResponse implementation

void CallToolResponse::addTextContent(const std::string& text) {
    content_.push_back(ToolContent::createText(text));
}

void CallToolResponse::addErrorContent(const std::string& error) {
    content_.push_back(ToolContent::createError(error));
    setIsError(true);
}

void CallToolResponse::addJsonContent(const std::string& json) {
    content_.push_back(ToolContent::createJson(json));
}

int CallToolResponse::serializeResult(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* result = cJSON_CreateObject();
    if (!result) return -1;
    
    // Add content array
    cJSON* contentArray = cJSON_CreateArray();
    if (!contentArray) {
        cJSON_Delete(result);
        return -1;
    }
    
    for (const auto& content : content_) {
        cJSON* contentJson = content.toJson();
        if (contentJson) {
            JsonHelper::addToArray(contentArray, contentJson);
        }
    }
    
    JsonHelper::setArray(result, MSG_KEY_CONTENT, contentArray);
    
    // Add error flag if needed
    if (isError_) {
        JsonHelper::setBool(result, MSG_KEY_IS_ERROR, true);
    }
    
    // Add progress information if available
    if (progress_ >= 0) {
        cJSON* meta = cJSON_CreateObject();
        if (meta) {
            JsonHelper::setInt(meta, MSG_KEY_PROGRESS, progress_);
            JsonHelper::setInt(meta, MSG_KEY_TOTAL, total_);
            JsonHelper::setObject(result, MSG_KEY_META, meta);
        }
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_RESULT, result)) {
        return -1;
    }
    
    return 0;
}

int CallToolResponse::deserializeResult(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    cJSON* result = JsonHelper::getObject(json, MSG_KEY_RESULT);
    if (!result) return TINYMCP_INVALID_REQUEST;
    
    // Extract content array
    cJSON* contentArray = JsonHelper::getArray(result, MSG_KEY_CONTENT);
    if (contentArray) {
        content_.clear();
        int arraySize = JsonHelper::getArraySize(contentArray);
        for (int i = 0; i < arraySize; i++) {
            cJSON* contentJson = JsonHelper::getArrayItem(contentArray, i);
            if (contentJson) {
                ToolContent content(ContentType::TEXT, "");
                if (content.fromJson(contentJson)) {
                    content_.push_back(std::move(content));
                }
            }
        }
    }
    
    // Extract error flag
    isError_ = JsonHelper::getBool(result, MSG_KEY_IS_ERROR, false);
    
    // Extract progress information
    if (JsonHelper::hasField(result, MSG_KEY_META)) {
        cJSON* meta = JsonHelper::getObject(result, MSG_KEY_META);
        if (meta) {
            progress_ = JsonHelper::getInt(meta, MSG_KEY_PROGRESS, -1);
            total_ = JsonHelper::getInt(meta, MSG_KEY_TOTAL, 100);
        }
    }
    
    return 0;
}

// PingResponse implementation

int PingResponse::serializeResult(cJSON* json) const {
    if (!json) return -1;
    
    cJSON* result = cJSON_CreateObject();
    if (!result) return -1;
    
    JsonHelper::setString(result, "status", status_);
    
    if (responseTimestamp_ > 0) {
        JsonHelper::setInt(result, "timestamp", static_cast<int>(responseTimestamp_));
    }
    
    if (!JsonHelper::setObject(json, MSG_KEY_RESULT, result)) {
        return -1;
    }
    
    return 0;
}

int PingResponse::deserializeResult(const cJSON* json) {
    if (!json) return TINYMCP_INVALID_REQUEST;
    
    cJSON* result = JsonHelper::getObject(json, MSG_KEY_RESULT);
    if (!result) return TINYMCP_INVALID_REQUEST;
    
    status_ = JsonHelper::getString(result, "status", "ok");
    responseTimestamp_ = static_cast<uint64_t>(JsonHelper::getInt(result, "timestamp", 0));
    
    return 0;
}

// ResponseFactory implementation

std::unique_ptr<Response> ResponseFactory::createFromJson(const std::string& jsonStr) {
    JsonValue json = JsonValue::parse(jsonStr);
    if (!json.isValid()) return nullptr;
    
    return createFromJson(json.get());
}

std::unique_ptr<Response> ResponseFactory::createFromJson(const cJSON* json) {
    if (!json || !JsonHelper::validateResponse(json)) return nullptr;
    
    // Extract ID
    MessageId id;
    if (!id.setFromJson(json)) return nullptr;
    
    std::unique_ptr<Response> response;
    
    if (isErrorResponse(json)) {
        auto errorResp = std::make_unique<ErrorResponse>(id, Error(0, ""));
        if (errorResp->deserialize(JsonHelper::toString(json)) == 0) {
            response = std::move(errorResp);
        }
    } else {
        // For success responses, we need more context to determine the exact type
        // This would typically be handled by matching with pending requests
        MessageType type = getResponseType(json);
        
        switch (type) {
            case MessageType::INITIALIZE_RESPONSE: {
                auto initResp = std::make_unique<InitializeResponse>(id);
                if (initResp->deserialize(JsonHelper::toString(json)) == 0) {
                    response = std::move(initResp);
                }
                break;
            }
            case MessageType::LIST_TOOLS_RESPONSE: {
                auto listResp = std::make_unique<ListToolsResponse>(id);
                if (listResp->deserialize(JsonHelper::toString(json)) == 0) {
                    response = std::move(listResp);
                }
                break;
            }
            case MessageType::CALL_TOOL_RESPONSE: {
                auto callResp = std::make_unique<CallToolResponse>(id);
                if (callResp->deserialize(JsonHelper::toString(json)) == 0) {
                    response = std::move(callResp);
                }
                break;
            }
            case MessageType::PING_RESPONSE: {
                auto pingResp = std::make_unique<PingResponse>(id);
                if (pingResp->deserialize(JsonHelper::toString(json)) == 0) {
                    response = std::move(pingResp);
                }
                break;
            }
            default:
                break;
        }
    }
    
    return response;
}

std::unique_ptr<InitializeResponse> ResponseFactory::createInitializeResponse(
    const MessageId& id,
    const ServerInfo& serverInfo,
    const ServerCapabilities& capabilities) {
    
    auto response = std::make_unique<InitializeResponse>(id);
    response->setServerInfo(serverInfo);
    response->setCapabilities(capabilities);
    return response;
}

std::unique_ptr<ListToolsResponse> ResponseFactory::createListToolsResponse(
    const MessageId& id,
    const std::vector<Tool>& tools,
    const std::string& nextCursor) {
    
    auto response = std::make_unique<ListToolsResponse>(id);
    response->setTools(tools);
    if (!nextCursor.empty()) {
        response->setNextCursor(nextCursor);
    }
    return response;
}

std::unique_ptr<CallToolResponse> ResponseFactory::createCallToolResponse(
    const MessageId& id,
    const std::vector<ToolContent>& content,
    bool isError) {
    
    auto response = std::make_unique<CallToolResponse>(id);
    response->setContent(content);
    response->setIsError(isError);
    return response;
}

std::unique_ptr<PingResponse> ResponseFactory::createPingResponse(const MessageId& id) {
    return std::make_unique<PingResponse>(id);
}

std::unique_ptr<ErrorResponse> ResponseFactory::createErrorResponse(
    const MessageId& id,
    int code,
    const std::string& message,
    const std::string& data) {
    
    return std::make_unique<ErrorResponse>(id, code, message, data);
}

std::unique_ptr<ErrorResponse> ResponseFactory::createParseError(const MessageId& id) {
    return createErrorResponse(id, TINYMCP_PARSE_ERROR, ERROR_MSG_PARSE_ERROR);
}

std::unique_ptr<ErrorResponse> ResponseFactory::createInvalidRequest(const MessageId& id) {
    return createErrorResponse(id, TINYMCP_INVALID_REQUEST, ERROR_MSG_INVALID_REQUEST);
}

std::unique_ptr<ErrorResponse> ResponseFactory::createMethodNotFound(const MessageId& id) {
    return createErrorResponse(id, TINYMCP_METHOD_NOT_FOUND, ERROR_MSG_METHOD_NOT_FOUND);
}

std::unique_ptr<ErrorResponse> ResponseFactory::createInvalidParams(const MessageId& id) {
    return createErrorResponse(id, TINYMCP_INVALID_PARAMS, ERROR_MSG_INVALID_PARAMS);
}

std::unique_ptr<ErrorResponse> ResponseFactory::createInternalError(const MessageId& id) {
    return createErrorResponse(id, TINYMCP_INTERNAL_ERROR, ERROR_MSG_INTERNAL_ERROR);
}

MessageType ResponseFactory::getResponseType(const cJSON* json) {
    // This would typically be determined by matching with pending requests
    // For now, return a default type
    return MessageType::UNKNOWN;
}

bool ResponseFactory::isErrorResponse(const cJSON* json) {
    return JsonHelper::hasField(json, MSG_KEY_ERROR);
}

} // namespace tinymcp