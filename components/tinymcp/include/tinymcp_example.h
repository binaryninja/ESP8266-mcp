#pragma once

// Example Usage of TinyMCP Core Message System
// Demonstrates how to use the new message architecture for ESP32/ESP8266

#include "tinymcp_request.h"
#include "tinymcp_response.h"
#include "tinymcp_notification.h"
#include "tinymcp_json.h"

namespace tinymcp {

// Example MCP Server Implementation using new message system
class ExampleMCPServer {
public:
    ExampleMCPServer() = default;
    ~ExampleMCPServer() = default;
    
    // Process incoming JSON message and return response
    std::string processMessage(const std::string& jsonMessage);
    
private:
    // Request handlers
    std::unique_ptr<Response> handleInitializeRequest(const InitializeRequest& request);
    std::unique_ptr<Response> handleListToolsRequest(const ListToolsRequest& request);
    std::unique_ptr<Response> handleCallToolRequest(const CallToolRequest& request);
    std::unique_ptr<Response> handlePingRequest(const PingRequest& request);
    
    // Tool implementations
    std::string executePingTool(const std::string& host);
    std::string executeEchoTool(const std::string& message);
    std::string executeStatusTool();
    
    // Server state
    bool initialized_ = false;
    ServerInfo serverInfo_{"TinyMCP-ESP", "1.0.0"};
    ServerCapabilities capabilities_;
    std::vector<Tool> availableTools_;
};

// Example Tool Registry
class ExampleToolRegistry {
public:
    ExampleToolRegistry();
    
    // Tool management
    const std::vector<Tool>& getTools() const { return tools_; }
    const Tool* getTool(const std::string& name) const;
    bool hasTool(const std::string& name) const;
    
    // Tool execution
    CallToolResponse executeTool(const CallToolRequest& request);
    
private:
    void registerBuiltinTools();
    std::vector<Tool> tools_;
};

// Example Client Implementation
class ExampleMCPClient {
public:
    ExampleMCPClient() : nextId_(1) {}
    
    // Initialize connection with server
    std::string createInitializeRequest();
    bool processInitializeResponse(const std::string& jsonResponse);
    
    // Send initialized notification
    std::string createInitializedNotification();
    
    // List available tools
    std::string createListToolsRequest();
    std::vector<Tool> processListToolsResponse(const std::string& jsonResponse);
    
    // Call a tool
    std::string createCallToolRequest(const std::string& toolName, 
                                     const std::vector<ToolArgument>& args);
    std::vector<ToolContent> processCallToolResponse(const std::string& jsonResponse);
    
    // Send ping
    std::string createPingRequest();
    bool processPingResponse(const std::string& jsonResponse);
    
private:
    int getNextId() { return nextId_++; }
    
    int nextId_;
    bool initialized_ = false;
    std::vector<Tool> availableTools_;
};

// Example Message Processor
class ExampleMessageProcessor {
public:
    // Message type detection
    static MessageCategory detectCategory(const std::string& jsonMessage);
    static MessageType detectType(const std::string& jsonMessage);
    
    // Message validation
    static bool validateMessage(const std::string& jsonMessage);
    static std::string getValidationError(const std::string& jsonMessage);
    
    // Message routing
    static std::string routeMessage(const std::string& jsonMessage, 
                                   ExampleMCPServer& server);
    
    // Error handling
    static std::string createErrorResponse(const MessageId& id, 
                                         int code, 
                                         const std::string& message);
    static std::string createParseErrorResponse();
    
private:
    static std::unique_ptr<Request> parseRequest(const std::string& jsonMessage);
    static std::unique_ptr<Response> parseResponse(const std::string& jsonMessage);
    static std::unique_ptr<Notification> parseNotification(const std::string& jsonMessage);
};

// Example Notification Handler
class ExampleNotificationHandler {
public:
    // Progress tracking
    void sendProgressNotification(const ProgressToken& token, 
                                 int progress, 
                                 int total,
                                 const std::string& message = "");
    
    // Cancellation handling
    void sendCancelledNotification(const std::string& requestId, 
                                  const std::string& reason = "");
    
    // Logging
    void sendLogNotification(LogNotification::LogLevel level,
                           const std::string& message,
                           const std::string& context = "");
    
    // Tool list changes
    void sendToolsListChanged(const std::vector<std::string>& added = {},
                             const std::vector<std::string>& removed = {},
                             const std::vector<std::string>& modified = {});
    
    // Callback for sending notifications
    std::function<void(const std::string&)> sendCallback;
};

// Usage Examples as inline functions

inline std::string createSimpleInitializeRequest() {
    auto request = RequestFactory::createInitializeRequest(
        MessageId("init-1"),
        PROTOCOL_VERSION,
        ClientInfo("ESP-Client", "1.0.0")
    );
    
    std::string json;
    if (request && request->serialize(json) == 0) {
        return json;
    }
    return "";
}

inline std::string createSimpleListToolsResponse(const MessageId& id) {
    std::vector<Tool> tools;
    tools.emplace_back("ping", "Ping a host to test connectivity");
    tools.emplace_back("echo", "Echo back the provided message");
    tools.emplace_back("status", "Get system status information");
    
    auto response = ResponseFactory::createListToolsResponse(id, tools);
    
    std::string json;
    if (response && response->serialize(json) == 0) {
        return json;
    }
    return "";
}

inline std::string createSimpleProgressNotification(const std::string& token, int progress) {
    auto notification = NotificationFactory::createProgressNotification(
        ProgressToken(token),
        progress,
        100,
        "Processing request..."
    );
    
    std::string json;
    if (notification && notification->serialize(json) == 0) {
        return json;
    }
    return "";
}

inline bool isValidMCPMessage(const std::string& jsonMessage) {
    MessageValidator validator;
    auto result = validator.validate(jsonMessage);
    return result.isValid;
}

} // namespace tinymcp