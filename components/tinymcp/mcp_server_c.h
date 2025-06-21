/*
 * Pure C MCP Server Implementation for ESP8266
 * 
 * This implementation uses:
 * - Static memory allocation (no malloc/free)
 * - Stack-allocated buffers with size limits
 * - cJSON for lightweight JSON parsing
 * - Pure C with minimal stack usage
 * 
 * Designed to work within ESP8266's 4KB stack limit.
 */

#ifndef MCP_SERVER_C_H
#define MCP_SERVER_C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Buffer size limits - carefully chosen for ESP8266 stack constraints
#define MCP_MAX_MESSAGE_SIZE    1024    // Maximum JSON message size
#define MCP_MAX_METHOD_SIZE     64      // Maximum method name length
#define MCP_MAX_ID_SIZE         32      // Maximum request ID length
#define MCP_MAX_PARAMS_SIZE     256     // Maximum params JSON size
#define MCP_MAX_RESPONSE_SIZE   1024    // Maximum response size
#define MCP_MAX_TOOL_NAME_SIZE  32      // Maximum tool name length
#define MCP_MAX_ERROR_MSG_SIZE  128     // Maximum error message size

// MCP Server state
typedef enum {
    MCP_STATE_UNINITIALIZED = 0,
    MCP_STATE_READY,
    MCP_STATE_RUNNING,
    MCP_STATE_STOPPED,
    MCP_STATE_ERROR
} mcp_server_state_t;

// MCP Request structure with static buffers
typedef struct {
    char method[MCP_MAX_METHOD_SIZE];
    char id[MCP_MAX_ID_SIZE];
    char params[MCP_MAX_PARAMS_SIZE];
    bool has_id;
    bool has_params;
} mcp_request_t;

// MCP Response structure with static buffer
typedef struct {
    char buffer[MCP_MAX_RESPONSE_SIZE];
    size_t length;
} mcp_response_t;

// Forward declaration
typedef struct mcp_server mcp_server_t;

// Transport interface - minimal function pointers
typedef struct {
    bool (*is_connected)(void* transport_data);
    bool (*read_message)(void* transport_data, char* buffer, size_t buffer_size, size_t* bytes_read);
    bool (*write_message)(void* transport_data, const char* message, size_t length);
    void (*close)(void* transport_data);
    void* transport_data;
} mcp_transport_t;

// MCP Server structure
struct mcp_server {
    mcp_transport_t* transport;
    mcp_server_state_t state;
    bool initialized;
    
    // Static buffers to avoid dynamic allocation
    char message_buffer[MCP_MAX_MESSAGE_SIZE];
    mcp_request_t current_request;
    mcp_response_t current_response;
    
    // Statistics
    uint32_t messages_processed;
    uint32_t errors_count;
};

// ESP8266 Socket Transport structure
typedef struct {
    int socket_fd;
    bool connected;
} esp_socket_transport_data_t;

// Function declarations

/**
 * Initialize MCP server with transport
 * Returns true on success, false on failure
 */
bool mcp_server_init(mcp_server_t* server, mcp_transport_t* transport);

/**
 * Run the MCP server main loop
 * This function blocks until the server is stopped or connection is lost
 * Returns true if stopped gracefully, false on error
 */
bool mcp_server_run(mcp_server_t* server);

/**
 * Stop the MCP server
 */
void mcp_server_stop(mcp_server_t* server);

/**
 * Check if server is running
 */
bool mcp_server_is_running(const mcp_server_t* server);

/**
 * Get current server state
 */
mcp_server_state_t mcp_server_get_state(const mcp_server_t* server);

/**
 * Parse JSON-RPC request from message buffer
 * Returns true on successful parse, false on error
 */
bool mcp_parse_request(const char* message, size_t length, mcp_request_t* request);

/**
 * Handle initialize request
 */
bool mcp_handle_initialize(const mcp_request_t* request, mcp_response_t* response);

/**
 * Handle tools/list request
 */
bool mcp_handle_tools_list(const mcp_request_t* request, mcp_response_t* response);

/**
 * Handle tools/call request
 */
bool mcp_handle_tools_call(const mcp_request_t* request, mcp_response_t* response);

/**
 * Handle ping request
 */
bool mcp_handle_ping(const mcp_request_t* request, mcp_response_t* response);

/**
 * Create error response
 */
bool mcp_create_error_response(const char* id, int error_code, const char* error_message, 
                              mcp_response_t* response);

/**
 * Create success response with result JSON
 */
bool mcp_create_success_response(const char* id, const char* result_json, 
                                mcp_response_t* response);

/**
 * ESP8266 Socket Transport Functions
 */

/**
 * Create ESP8266 socket transport
 * Returns true on success, false on failure
 */
bool esp_socket_transport_create(mcp_transport_t* transport, int socket_fd);

/**
 * ESP8266 socket transport implementation functions
 */
bool esp_socket_is_connected(void* transport_data);
bool esp_socket_read_message(void* transport_data, char* buffer, size_t buffer_size, size_t* bytes_read);
bool esp_socket_write_message(void* transport_data, const char* message, size_t length);
void esp_socket_close(void* transport_data);

/**
 * Utility functions
 */

/**
 * Safe string copy with size limit
 */
void mcp_safe_strcpy(char* dest, const char* src, size_t dest_size);

/**
 * Safe string concatenation with size limit
 */
void mcp_safe_strcat(char* dest, const char* src, size_t dest_size);

/**
 * Get memory usage statistics
 */
void mcp_get_memory_stats(size_t* free_heap, size_t* min_free_heap, size_t* stack_remaining);

/**
 * Log memory usage with context
 */
void mcp_log_memory_usage(const char* context);

#ifdef __cplusplus
}
#endif

#endif // MCP_SERVER_C_H