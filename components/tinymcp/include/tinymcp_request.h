#pragma once

// Request Message Classes for TinyMCP Protocol
// JSON-RPC 2.0 compliant request messages for ESP32/ESP8266

#include <string>
#include <memory>
#include <vector>
#include <cJSON.h>
#include "tinymcp_message.h"
#include "tinymcp_constants.h"
#include "tinymcp_json.h"

namespace tinymcp {

// Base Request class
class Request : public Message {
public:
    Request(MessageType type, const MessageId& id, const std::string& method);
    virtual ~Request() = default;
    
    // Move constructor and assignment
    Request(Request&&) = default;
    Request& operator=(Request&&) = default;
    
    // Getters
    const MessageId& getId() const { return id_; }
    const std::string& getMethod() const { return method_; }
    
    // Setters
    void setId(const MessageId& id) { id_ = id; }
    void setMethod(const std::string& method) { method_ = method; }
    
    // Base Message interface implementation
    bool isValid() const override;
    int serialize(std::string& jsonOut) const override;
    int deserialize(const std::string& jsonIn) override;
    
    // Request-specific validation
    virtual bool validateParams(const cJSON* params) const { return true; }
    
protected:
    int doSerialize(cJSON* json) const override;
    int doDeserialize(const cJSON* json) override;
    
    // Virtual methods for parameter handling
    virtual int serializeParams(cJSON* json) const { return 0; }
    virtual int deserializeParams(const cJSON* json) { return 0; }
    
    MessageId id_;
    std::string method_;
};

// Initialize Request
class InitializeRequest : public Request {
public:
    InitializeRequest(const MessageId& id = MessageId("init"))
        : Request(MessageType::INITIALIZE_REQUEST, id, METHOD_INITIALIZE) {}
    
    // Protocol version
    const std::string& getProtocolVersion() const { return protocolVersion_; }
    void setProtocolVersion(const std::string& version) { protocolVersion_ = version; }
    
    // Client information
    const ClientInfo& getClientInfo() const { return clientInfo_; }
    void setClientInfo(const ClientInfo& info) { clientInfo_ = info; }
    
    // Client capabilities (future expansion)
    bool hasClientCapabilities() const { return clientCapabilities_ != nullptr; }
    const cJSON* getClientCapabilities() const { return clientCapabilities_; }
    void setClientCapabilities(cJSON* capabilities);
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    std::string protocolVersion_;
    ClientInfo clientInfo_{"", ""};
    cJSON* clientCapabilities_;
    
public:
    ~InitializeRequest() {
        if (clientCapabilities_) {
            cJSON_Delete(clientCapabilities_);
        }
    }
};

// List Tools Request
class ListToolsRequest : public Request {
public:
    ListToolsRequest(const MessageId& id)
        : Request(MessageType::LIST_TOOLS_REQUEST, id, METHOD_TOOLS_LIST)
        , cursor_("")
        , maxResults_(0) {}
    
    // Pagination support
    const std::string& getCursor() const { return cursor_; }
    void setCursor(const std::string& cursor) { cursor_ = cursor; }
    
    int getMaxResults() const { return maxResults_; }
    void setMaxResults(int maxResults) { maxResults_ = maxResults; }
    
    bool hasPagination() const { return !cursor_.empty() || maxResults_ > 0; }
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    std::string cursor_;
    int maxResults_;
};

// Tool argument class for Call Tool Request
class ToolArgument {
public:
    ToolArgument(const std::string& name, const std::string& value)
        : name_(name), value_(value) {}
    
    const std::string& getName() const { return name_; }
    const std::string& getValue() const { return value_; }
    
    void setName(const std::string& name) { name_ = name; }
    void setValue(const std::string& value) { value_ = value; }
    
    bool isValid() const { return !name_.empty(); }
    
    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

private:
    std::string name_;
    std::string value_;
};

// Call Tool Request
class CallToolRequest : public Request {
public:
    CallToolRequest(const MessageId& id, const std::string& toolName)
        : Request(MessageType::CALL_TOOL_REQUEST, id, METHOD_TOOLS_CALL)
        , toolName_(toolName) {}
    
    // Tool information
    const std::string& getToolName() const { return toolName_; }
    void setToolName(const std::string& name) { toolName_ = name; }
    
    // Arguments management
    void addArgument(const std::string& name, const std::string& value);
    void setArguments(const std::vector<ToolArgument>& args) { arguments_ = args; }
    const std::vector<ToolArgument>& getArguments() const { return arguments_; }
    
    bool hasArgument(const std::string& name) const;
    std::string getArgumentValue(const std::string& name) const;
    
    // Raw arguments (for complex JSON structures)
    void setRawArguments(cJSON* args);
    const cJSON* getRawArguments() const { return rawArguments_; }
    bool hasRawArguments() const { return rawArguments_ != nullptr; }
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    std::string toolName_;
    std::vector<ToolArgument> arguments_;
    cJSON* rawArguments_;
    
public:
    ~CallToolRequest() {
        if (rawArguments_) {
            cJSON_Delete(rawArguments_);
        }
    }
    
    // Copy constructor and assignment for argument management
    CallToolRequest(const CallToolRequest& other);
    CallToolRequest& operator=(const CallToolRequest& other);
};

// Ping Request (for connection testing)
class PingRequest : public Request {
public:
    PingRequest(const MessageId& id)
        : Request(MessageType::PING_REQUEST, id, METHOD_PING) {}
    
    // Ping requests typically have no parameters
    bool validateParams(const cJSON* params) const override {
        return true; // Ping has no required parameters
    }
};

// Request Factory for creating requests from JSON
class RequestFactory {
public:
    static std::unique_ptr<Request> createFromJson(const std::string& jsonStr);
    static std::unique_ptr<Request> createFromJson(const cJSON* json);
    
    // Factory methods for specific request types
    static std::unique_ptr<InitializeRequest> createInitializeRequest(
        const MessageId& id,
        const std::string& protocolVersion,
        const ClientInfo& clientInfo);
    
    static std::unique_ptr<ListToolsRequest> createListToolsRequest(
        const MessageId& id,
        const std::string& cursor = "",
        int maxResults = 0);
    
    static std::unique_ptr<CallToolRequest> createCallToolRequest(
        const MessageId& id,
        const std::string& toolName,
        const std::vector<ToolArgument>& arguments = {});
    
    static std::unique_ptr<PingRequest> createPingRequest(const MessageId& id);
    
private:
    static MessageType getRequestType(const std::string& method);
};

// Request validation utilities
class RequestValidator {
public:
    static bool validateInitializeRequest(const cJSON* json);
    static bool validateListToolsRequest(const cJSON* json);
    static bool validateCallToolRequest(const cJSON* json);
    static bool validatePingRequest(const cJSON* json);
    
    // Parameter validation helpers
    static bool validateProtocolVersion(const std::string& version);
    static bool validateClientInfo(const cJSON* clientInfo);
    static bool validateToolName(const std::string& toolName);
    static bool validateToolArguments(const cJSON* arguments);
    
private:
    static bool isValidToolNameChar(char c);
    static bool isValidParameterName(const std::string& name);
};

// Request builder pattern for easier construction
class RequestBuilder {
public:
    RequestBuilder() = default;
    
    // Initialize request builder
    RequestBuilder& initializeRequest(const MessageId& id);
    RequestBuilder& withProtocolVersion(const std::string& version);
    RequestBuilder& withClientInfo(const std::string& name, const std::string& version);
    RequestBuilder& withClientCapabilities(cJSON* capabilities);
    
    // List tools request builder
    RequestBuilder& listToolsRequest(const MessageId& id);
    RequestBuilder& withCursor(const std::string& cursor);
    RequestBuilder& withMaxResults(int maxResults);
    
    // Call tool request builder
    RequestBuilder& callToolRequest(const MessageId& id, const std::string& toolName);
    RequestBuilder& withArgument(const std::string& name, const std::string& value);
    RequestBuilder& withRawArguments(cJSON* arguments);
    
    // Ping request builder
    RequestBuilder& pingRequest(const MessageId& id);
    
    // Progress token (can be added to any request)
    RequestBuilder& withProgressToken(const std::string& token);
    
    // Build the final request
    std::unique_ptr<Request> build();
    
    // Reset for reuse
    void reset();

private:
    MessageType requestType_ = MessageType::UNKNOWN;
    MessageId id_;
    std::string method_;
    std::string protocolVersion_;
    ClientInfo clientInfo_{"", ""};
    cJSON* clientCapabilities_ = nullptr;
    std::string cursor_;
    int maxResults_ = 0;
    std::string toolName_;
    std::vector<ToolArgument> arguments_;
    cJSON* rawArguments_ = nullptr;
    std::string progressToken_;
};

} // namespace tinymcp