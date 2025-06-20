#pragma once

// TinyMCP Protocol Constants and Error Codes
// JSON-RPC 2.0 compliant implementation for ESP32/ESP8266

#include <string>

namespace tinymcp {

// Protocol Version
static constexpr const char* PROTOCOL_VERSION = "2024-11-05";
static constexpr const char* JSON_RPC_VERSION = "2.0";

// JSON-RPC 2.0 Message Keys
static constexpr const char* MSG_KEY_JSONRPC = "jsonrpc";
static constexpr const char* MSG_KEY_ID = "id";
static constexpr const char* MSG_KEY_METHOD = "method";
static constexpr const char* MSG_KEY_PARAMS = "params";
static constexpr const char* MSG_KEY_RESULT = "result";
static constexpr const char* MSG_KEY_ERROR = "error";
static constexpr const char* MSG_KEY_CODE = "code";
static constexpr const char* MSG_KEY_MESSAGE = "message";
static constexpr const char* MSG_KEY_DATA = "data";

// MCP Protocol Specific Keys
static constexpr const char* MSG_KEY_PROTOCOL_VERSION = "protocolVersion";
static constexpr const char* MSG_KEY_CLIENT_INFO = "clientInfo";
static constexpr const char* MSG_KEY_SERVER_INFO = "serverInfo";
static constexpr const char* MSG_KEY_NAME = "name";
static constexpr const char* MSG_KEY_VERSION = "version";
static constexpr const char* MSG_KEY_CAPABILITIES = "capabilities";
static constexpr const char* MSG_KEY_TOOLS = "tools";
static constexpr const char* MSG_KEY_DESCRIPTION = "description";
static constexpr const char* MSG_KEY_INPUT_SCHEMA = "inputSchema";
static constexpr const char* MSG_KEY_ARGUMENTS = "arguments";
static constexpr const char* MSG_KEY_CONTENT = "content";
static constexpr const char* MSG_KEY_TEXT = "text";
static constexpr const char* MSG_KEY_TYPE = "type";
static constexpr const char* MSG_KEY_IS_ERROR = "isError";
static constexpr const char* MSG_KEY_PROGRESS_TOKEN = "progressToken";
static constexpr const char* MSG_KEY_PROGRESS = "progress";
static constexpr const char* MSG_KEY_TOTAL = "total";
static constexpr const char* MSG_KEY_CURSOR = "cursor";
static constexpr const char* MSG_KEY_NEXT_CURSOR = "nextCursor";
static constexpr const char* MSG_KEY_LISTCHANGED = "listChanged";
static constexpr const char* MSG_KEY_MIMETYPE = "mimeType";
static constexpr const char* MSG_KEY_META = "_meta";

// MCP Methods
static constexpr const char* METHOD_INITIALIZE = "initialize";
static constexpr const char* METHOD_INITIALIZED = "notifications/initialized";
static constexpr const char* METHOD_TOOLS_LIST = "tools/list";
static constexpr const char* METHOD_TOOLS_CALL = "tools/call";
static constexpr const char* METHOD_PROGRESS = "notifications/progress";
static constexpr const char* METHOD_CANCELLED = "notifications/cancelled";
static constexpr const char* METHOD_PING = "ping";

// JSON-RPC 2.0 Standard Error Codes
static constexpr int TINYMCP_PARSE_ERROR = -32700;
static constexpr int TINYMCP_INVALID_REQUEST = -32600;
static constexpr int TINYMCP_METHOD_NOT_FOUND = -32601;
static constexpr int TINYMCP_INVALID_PARAMS = -32602;
static constexpr int TINYMCP_INTERNAL_ERROR = -32603;

// JSON-RPC 2.0 Server Error Range (-32000 to -32099)
static constexpr int TINYMCP_SERVER_ERROR_START = -32099;
static constexpr int TINYMCP_SERVER_ERROR_END = -32000;

// MCP Specific Error Codes
static constexpr int TINYMCP_NOT_INITIALIZED = -32001;
static constexpr int TINYMCP_INVALID_NOTIFICATION = -32002;
static constexpr int TINYMCP_TOOL_ERROR = -32003;
static constexpr int TINYMCP_CANCELLED = -32004;
static constexpr int TINYMCP_TIMEOUT = -32005;

// Success and General Error Codes
static constexpr int TINYMCP_SUCCESS = 0;
static constexpr int TINYMCP_ERROR_CANCELLED = TINYMCP_CANCELLED;
static constexpr int TINYMCP_ERROR_NO_PROGRESS_TOKEN = -32006;
static constexpr int TINYMCP_ERROR_OUT_OF_MEMORY = -32007;
static constexpr int TINYMCP_ERROR_INVALID_STATE = -32008;
static constexpr int TINYMCP_ERROR_TRANSPORT_FAILED = -32009;
static constexpr int TINYMCP_ERROR_TASK_CREATION_FAILED = -32010;
static constexpr int TINYMCP_ERROR_TIMEOUT = TINYMCP_TIMEOUT;
static constexpr int TINYMCP_ERROR_INVALID_PARAMS = TINYMCP_INVALID_PARAMS;
static constexpr int TINYMCP_ERROR_RESOURCE_LIMIT = -32011;
static constexpr int TINYMCP_ERROR_MESSAGE_TOO_LARGE = -32012;
static constexpr int TINYMCP_ERROR_INVALID_OPERATION = -32013;
static constexpr int TINYMCP_ERROR_NOT_IMPLEMENTED = -32014;
static constexpr int TINYMCP_ERROR_INVALID_MESSAGE = -32015;
static constexpr int TINYMCP_ERROR_RESOURCE_LOCK = -32016;
static constexpr int TINYMCP_ERROR_NOT_FOUND = -32017;
static constexpr int TINYMCP_ERROR_METHOD_NOT_FOUND = TINYMCP_METHOD_NOT_FOUND;
static constexpr int TINYMCP_ERROR_NOT_INITIALIZED = TINYMCP_NOT_INITIALIZED;
static constexpr int TINYMCP_ERROR_TOOL_NOT_FOUND = -32018;
static constexpr int TINYMCP_ERROR_HARDWARE_FAILED = -32019;

// Error Messages
static constexpr const char* ERROR_MSG_PARSE_ERROR = "Parse error";
static constexpr const char* ERROR_MSG_INVALID_REQUEST = "Invalid Request";
static constexpr const char* ERROR_MSG_METHOD_NOT_FOUND = "Method not found";
static constexpr const char* ERROR_MSG_INVALID_PARAMS = "Invalid params";
static constexpr const char* ERROR_MSG_INTERNAL_ERROR = "Internal error";
static constexpr const char* ERROR_MSG_NOT_INITIALIZED = "Server not initialized";
static constexpr const char* ERROR_MSG_INVALID_NOTIFICATION = "Invalid notification";
static constexpr const char* ERROR_MSG_TOOL_ERROR = "Tool execution error";
static constexpr const char* ERROR_MSG_CANCELLED = "Request cancelled";
static constexpr const char* ERROR_MSG_TIMEOUT = "Request timeout";

// Data Types
enum class DataType {
    UNKNOWN,
    STRING,
    INTEGER,
    BOOLEAN,
    OBJECT,
    ARRAY
};

// Message Categories
enum class MessageCategory {
    UNKNOWN,
    REQUEST,
    RESPONSE,
    NOTIFICATION
};

// Message Types
enum class MessageType {
    UNKNOWN,
    
    // Request types
    INITIALIZE_REQUEST,
    LIST_TOOLS_REQUEST,
    CALL_TOOL_REQUEST,
    PING_REQUEST,
    
    // Response types
    INITIALIZE_RESPONSE,
    LIST_TOOLS_RESPONSE,
    CALL_TOOL_RESPONSE,
    PING_RESPONSE,
    ERROR_RESPONSE,
    
    // Notification types
    INITIALIZED_NOTIFICATION,
    PROGRESS_NOTIFICATION,
    CANCELLED_NOTIFICATION
};

// Content Types
enum class ContentType {
    TEXT,
    IMAGE,
    RESOURCE
};

// Tool Result Types
enum class ToolResultType {
    SUCCESS,
    ERROR
};

// Maximum limits for ESP32/ESP8266
static constexpr size_t MAX_MESSAGE_SIZE = 8192;
static constexpr size_t MAX_METHOD_NAME_LENGTH = 64;
static constexpr size_t MAX_TOOL_NAME_LENGTH = 64;
static constexpr size_t MAX_ERROR_MESSAGE_LENGTH = 256;
static constexpr size_t MAX_TOOLS_COUNT = 16;
static constexpr size_t MAX_CONTENT_LENGTH = 4096;

// Default timeouts (milliseconds)
static constexpr uint32_t DEFAULT_REQUEST_TIMEOUT_MS = 30000;
static constexpr uint32_t DEFAULT_TOOL_TIMEOUT_MS = 60000;
static constexpr uint32_t DEFAULT_PROGRESS_INTERVAL_MS = 1000;

} // namespace tinymcp