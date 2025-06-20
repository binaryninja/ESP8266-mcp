#pragma once

// Response Message Classes for TinyMCP Protocol
// JSON-RPC 2.0 compliant response messages for ESP32/ESP8266

#include <string>
#include <memory>
#include <vector>
#include <cJSON.h>
#include "tinymcp_message.h"
#include "tinymcp_constants.h"
#include "tinymcp_json.h"

namespace tinymcp {

// Base Response class
class Response : public Message {
public:
    Response(MessageType type, const MessageId& id);
    virtual ~Response() = default;
    
    // Move constructor and assignment
    Response(Response&&) = default;
    Response& operator=(Response&&) = default;
    
    // Getters
    const MessageId& getId() const { return id_; }
    bool isError() const { return isError_; }
    
    // Setters
    void setId(const MessageId& id) { id_ = id; }
    
    // Base Message interface implementation
    bool isValid() const override;
    int serialize(std::string& jsonOut) const override;
    int deserialize(const std::string& jsonIn) override;
    
protected:
    int doSerialize(cJSON* json) const override;
    int doDeserialize(const cJSON* json) override;
    
    // Virtual methods for result/error handling
    virtual int serializeResult(cJSON* json) const { return 0; }
    virtual int deserializeResult(const cJSON* json) { return 0; }
    
    MessageId id_;
    bool isError_;
};

// Success Response (base for all successful responses)
class SuccessResponse : public Response {
public:
    SuccessResponse(MessageType type, const MessageId& id)
        : Response(type, id) {
        isError_ = false;
    }
};

// Error Response
class ErrorResponse : public Response {
public:
    ErrorResponse(const MessageId& id, const Error& error)
        : Response(MessageType::ERROR_RESPONSE, id), error_(error) {
        isError_ = true;
    }
    
    ErrorResponse(const MessageId& id, int code, const std::string& message, const std::string& data = "")
        : Response(MessageType::ERROR_RESPONSE, id), error_(code, message, data) {
        isError_ = true;
    }
    
    // Error information
    const Error& getError() const { return error_; }
    void setError(const Error& error) { error_ = error; }
    
    int getErrorCode() const { return error_.getCode(); }
    const std::string& getErrorMessage() const { return error_.getMessage(); }
    const std::string& getErrorData() const { return error_.getData(); }
    
protected:
    int serializeResult(cJSON* json) const override;
    int deserializeResult(const cJSON* json) override;

private:
    Error error_;
};

// Initialize Response
class InitializeResponse : public SuccessResponse {
public:
    InitializeResponse(const MessageId& id)
        : SuccessResponse(MessageType::INITIALIZE_RESPONSE, id)
        , protocolVersion_(PROTOCOL_VERSION)
        , serverInfo_("TinyMCP-ESP", "1.0.0") {}
    
    // Protocol version
    const std::string& getProtocolVersion() const { return protocolVersion_; }
    void setProtocolVersion(const std::string& version) { protocolVersion_ = version; }
    
    // Server information
    const ServerInfo& getServerInfo() const { return serverInfo_; }
    void setServerInfo(const ServerInfo& info) { serverInfo_ = info; }
    
    // Server capabilities
    const ServerCapabilities& getCapabilities() const { return capabilities_; }
    void setCapabilities(const ServerCapabilities& capabilities) { capabilities_ = capabilities; }
    
    // Instructions (optional)
    const std::string& getInstructions() const { return instructions_; }
    void setInstructions(const std::string& instructions) { instructions_ = instructions; }
    bool hasInstructions() const { return !instructions_.empty(); }
    
protected:
    int serializeResult(cJSON* json) const override;
    int deserializeResult(const cJSON* json) override;

private:
    std::string protocolVersion_;
    ServerInfo serverInfo_;
    ServerCapabilities capabilities_;
    std::string instructions_;
};

// Tool definition for List Tools Response
class Tool {
public:
    Tool(const std::string& name, const std::string& description)
        : name_(name), description_(description), inputSchema_(nullptr) {}
    
    ~Tool() {
        if (inputSchema_) {
            cJSON_Delete(inputSchema_);
        }
    }
    
    // Copy constructor and assignment
    Tool(const Tool& other);
    Tool& operator=(const Tool& other);
    
    // Move constructor and assignment
    Tool(Tool&& other) noexcept;
    Tool& operator=(Tool&& other) noexcept;
    
    // Basic properties
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    
    void setName(const std::string& name) { name_ = name; }
    void setDescription(const std::string& description) { description_ = description; }
    
    // Input schema
    const cJSON* getInputSchema() const { return inputSchema_; }
    void setInputSchema(cJSON* schema);
    bool hasInputSchema() const { return inputSchema_ != nullptr; }
    
    // Validation
    bool isValid() const { return !name_.empty() && !description_.empty(); }
    
    // Serialization
    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

private:
    std::string name_;
    std::string description_;
    cJSON* inputSchema_;
    
    void copyInputSchema(const cJSON* schema);
};

// List Tools Response
class ListToolsResponse : public SuccessResponse {
public:
    ListToolsResponse(const MessageId& id)
        : SuccessResponse(MessageType::LIST_TOOLS_RESPONSE, id) {}
    
    // Tools management
    void addTool(const Tool& tool) { tools_.push_back(tool); }
    void setTools(const std::vector<Tool>& tools) { tools_ = tools; }
    const std::vector<Tool>& getTools() const { return tools_; }
    size_t getToolCount() const { return tools_.size(); }
    
    // Pagination support
    const std::string& getNextCursor() const { return nextCursor_; }
    void setNextCursor(const std::string& cursor) { nextCursor_ = cursor; }
    bool hasNextCursor() const { return !nextCursor_.empty(); }
    
    // Tool lookup
    bool hasTool(const std::string& name) const;
    const Tool* getTool(const std::string& name) const;
    
protected:
    int serializeResult(cJSON* json) const override;
    int deserializeResult(const cJSON* json) override;

private:
    std::vector<Tool> tools_;
    std::string nextCursor_;
};

// Tool execution result content
class ToolContent {
public:
    ToolContent(ContentType type, const std::string& text)
        : type_(type), text_(text), mimeType_("text/plain") {}
    
    ToolContent(ContentType type, const std::string& text, const std::string& mimeType)
        : type_(type), text_(text), mimeType_(mimeType) {}
    
    ContentType getType() const { return type_; }
    const std::string& getText() const { return text_; }
    const std::string& getMimeType() const { return mimeType_; }
    
    void setType(ContentType type) { type_ = type; }
    void setText(const std::string& text) { text_ = text; }
    void setMimeType(const std::string& mimeType) { mimeType_ = mimeType; }
    
    bool isValid() const { return !text_.empty(); }
    
    cJSON* toJson() const;
    bool fromJson(const cJSON* json);
    
    // Factory methods
    static ToolContent createText(const std::string& text);
    static ToolContent createError(const std::string& error);
    static ToolContent createJson(const std::string& json);

private:
    ContentType type_;
    std::string text_;
    std::string mimeType_;
};

// Call Tool Response
class CallToolResponse : public SuccessResponse {
public:
    CallToolResponse(const MessageId& id)
        : SuccessResponse(MessageType::CALL_TOOL_RESPONSE, id), isError_(false) {}
    
    // Content management
    void addContent(const ToolContent& content) { content_.push_back(content); }
    void setContent(const std::vector<ToolContent>& content) { content_ = content; }
    const std::vector<ToolContent>& getContent() const { return content_; }
    
    // Error handling
    bool isToolError() const { return isError_; }
    void setIsError(bool error) { isError_ = error; }
    
    // Progress information (for async operations)
    bool hasProgress() const { return progress_ >= 0; }
    int getProgress() const { return progress_; }
    int getTotal() const { return total_; }
    void setProgress(int progress, int total = 100) { 
        progress_ = progress; 
        total_ = total; 
    }
    
    // Convenience methods
    void addTextContent(const std::string& text);
    void addErrorContent(const std::string& error);
    void addJsonContent(const std::string& json);
    
protected:
    int serializeResult(cJSON* json) const override;
    int deserializeResult(const cJSON* json) override;

private:
    std::vector<ToolContent> content_;
    bool isError_;
    int progress_ = -1;
    int total_ = 100;
};

// Ping Response
class PingResponse : public SuccessResponse {
public:
    PingResponse(const MessageId& id)
        : SuccessResponse(MessageType::PING_RESPONSE, id) {}
    
    // Ping responses can include server status
    const std::string& getStatus() const { return status_; }
    void setStatus(const std::string& status) { status_ = status; }
    
    // Timestamp for latency measurement
    uint64_t getTimestamp() const { return responseTimestamp_; }
    void setTimestamp(uint64_t timestamp) { responseTimestamp_ = timestamp; }
    
protected:
    int serializeResult(cJSON* json) const override;
    int deserializeResult(const cJSON* json) override;

private:
    std::string status_ = "ok";
    uint64_t responseTimestamp_ = 0;
};

// Response Factory for creating responses from JSON
class ResponseFactory {
public:
    static std::unique_ptr<Response> createFromJson(const std::string& jsonStr);
    static std::unique_ptr<Response> createFromJson(const cJSON* json);
    
    // Factory methods for specific response types
    static std::unique_ptr<InitializeResponse> createInitializeResponse(
        const MessageId& id,
        const ServerInfo& serverInfo,
        const ServerCapabilities& capabilities);
    
    static std::unique_ptr<ListToolsResponse> createListToolsResponse(
        const MessageId& id,
        const std::vector<Tool>& tools,
        const std::string& nextCursor = "");
    
    static std::unique_ptr<CallToolResponse> createCallToolResponse(
        const MessageId& id,
        const std::vector<ToolContent>& content,
        bool isError = false);
    
    static std::unique_ptr<PingResponse> createPingResponse(const MessageId& id);
    
    static std::unique_ptr<ErrorResponse> createErrorResponse(
        const MessageId& id,
        int code,
        const std::string& message,
        const std::string& data = "");
    
    // Standard error responses
    static std::unique_ptr<ErrorResponse> createParseError(const MessageId& id = MessageId(""));
    static std::unique_ptr<ErrorResponse> createInvalidRequest(const MessageId& id = MessageId(""));
    static std::unique_ptr<ErrorResponse> createMethodNotFound(const MessageId& id);
    static std::unique_ptr<ErrorResponse> createInvalidParams(const MessageId& id);
    static std::unique_ptr<ErrorResponse> createInternalError(const MessageId& id);

private:
    static MessageType getResponseType(const cJSON* json);
    static bool isErrorResponse(const cJSON* json);
};

// Response validation utilities
class ResponseValidator {
public:
    static bool validateInitializeResponse(const cJSON* json);
    static bool validateListToolsResponse(const cJSON* json);
    static bool validateCallToolResponse(const cJSON* json);
    static bool validatePingResponse(const cJSON* json);
    static bool validateErrorResponse(const cJSON* json);
    
    // Specific validation helpers
    static bool validateServerInfo(const cJSON* serverInfo);
    static bool validateCapabilities(const cJSON* capabilities);
    static bool validateTool(const cJSON* tool);
    static bool validateToolContent(const cJSON* content);

private:
    static bool isValidMimeType(const std::string& mimeType);
    static bool isValidContentType(const std::string& type);
};

// Response builder pattern for easier construction
class ResponseBuilder {
public:
    ResponseBuilder() = default;
    
    // Initialize response builder
    ResponseBuilder& initializeResponse(const MessageId& id);
    ResponseBuilder& withServerInfo(const std::string& name, const std::string& version);
    ResponseBuilder& withCapabilities(const ServerCapabilities& capabilities);
    ResponseBuilder& withInstructions(const std::string& instructions);
    
    // List tools response builder
    ResponseBuilder& listToolsResponse(const MessageId& id);
    ResponseBuilder& withTool(const Tool& tool);
    ResponseBuilder& withTools(const std::vector<Tool>& tools);
    ResponseBuilder& withNextCursor(const std::string& cursor);
    
    // Call tool response builder
    ResponseBuilder& callToolResponse(const MessageId& id);
    ResponseBuilder& withContent(const ToolContent& content);
    ResponseBuilder& withTextContent(const std::string& text);
    ResponseBuilder& withErrorContent(const std::string& error);
    ResponseBuilder& withProgress(int progress, int total = 100);
    ResponseBuilder& asError(bool isError = true);
    
    // Ping response builder
    ResponseBuilder& pingResponse(const MessageId& id);
    ResponseBuilder& withStatus(const std::string& status);
    
    // Error response builder
    ResponseBuilder& errorResponse(const MessageId& id, int code, const std::string& message);
    ResponseBuilder& withErrorData(const std::string& data);
    
    // Build the final response
    std::unique_ptr<Response> build();
    
    // Reset for reuse
    void reset();

private:
    MessageType responseType_ = MessageType::UNKNOWN;
    MessageId id_;
    
    // Initialize response data
    ServerInfo serverInfo_{"", ""};
    ServerCapabilities capabilities_;
    std::string instructions_;
    
    // List tools response data
    std::vector<Tool> tools_;
    std::string nextCursor_;
    
    // Call tool response data
    std::vector<ToolContent> content_;
    bool isToolError_ = false;
    int progress_ = -1;
    int total_ = 100;
    
    // Ping response data
    std::string status_;
    
    // Error response data
    int errorCode_ = 0;
    std::string errorMessage_;
    std::string errorData_;
};

} // namespace tinymcp