#pragma once

#include <string>
#include <memory>
#include "EspSocketTransport.h"

// Forward declarations
typedef enum {
    JSON_OP_INITIALIZE,
    JSON_OP_TOOLS_LIST,
    JSON_OP_TOOLS_CALL,
    JSON_OP_PING,
    JSON_OP_ERROR_RESPONSE,
    JSON_OP_SUCCESS_RESPONSE
} json_operation_type_t;

namespace tinymcp {

class MCPServer {
public:
    explicit MCPServer(Transport* transport);
    ~MCPServer();
    
    // Delete copy constructor and assignment operator
    MCPServer(const MCPServer&) = delete;
    MCPServer& operator=(const MCPServer&) = delete;
    
    // Main server loop - blocks until client disconnects
    void run();
    
    // Stop the server
    void stop();
    
    // Check if server is running
    bool isRunning() const;

private:
    // Process incoming messages
    void processMessage(const std::string& message);
    
    // Send response back to client
    void sendResponse(const std::string& response);
    
    // Handle initialization request
    std::string handleInitialize(const std::string& request);
    
    // Handle tools/list request
    std::string handleToolsList(const std::string& request);
    
    // Handle tools/call request
    std::string handleToolsCall(const std::string& request);
    
    // Handle ping request
    std::string handlePing(const std::string& request);
    
    // Create error response
    std::string createErrorResponse(const std::string& id, int code, const std::string& message);
    
    // Create success response
    std::string createSuccessResponse(const std::string& id, const std::string& result);
    
    // Parse JSON-RPC request
    bool parseRequest(const std::string& message, std::string& method, std::string& id, std::string& params);
    
    // Helper function for JSON processing with separate task
    std::string processJsonOperation(json_operation_type_t operation, const std::string& input, 
                                   const std::string& param1 = "", const std::string& param2 = "", int param_int = 0);
    
    Transport* transport_;
    bool running_;
    bool initialized_;
    
    // Rate limiting and error tracking
    unsigned int error_count_;
    unsigned long last_error_time_;
    static const unsigned int MAX_ERRORS_PER_SECOND = 10;
    static const unsigned int MAX_TOTAL_ERRORS = 50;
};

} // namespace tinymcp