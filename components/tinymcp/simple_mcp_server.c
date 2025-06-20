#include "simple_mcp_server.h"
#include "cJSON.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "MCP_SERVER";

#define MAX_BUFFER_SIZE 2048
#define MAX_RESPONSE_SIZE 1024

// Helper function to create error response
static char* create_error_response(const char* id, int code, const char* message) {
    cJSON *response = cJSON_CreateObject();
    cJSON *error = cJSON_CreateObject();

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (id && strlen(id) > 0) {
        cJSON_AddStringToObject(response, "id", id);
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(response, "error", error);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    return response_str;
}

// Handle initialize request
static char* handle_initialize(const char* request_data) {
    cJSON *request = cJSON_Parse(request_data);
    if (!request) {
        return create_error_response("", -32700, "Parse error");
    }

    cJSON *id_item = cJSON_GetObjectItem(request, "id");
    const char* id = id_item ? cJSON_GetStringValue(id_item) : "";

    cJSON *response = cJSON_CreateObject();
    cJSON *result = cJSON_CreateObject();
    cJSON *server_info = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateObject();

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddStringToObject(response, "id", id);

    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");

    cJSON_AddStringToObject(server_info, "name", "ESP8266-MCP");
    cJSON_AddStringToObject(server_info, "version", "1.0.0");
    cJSON_AddItemToObject(result, "serverInfo", server_info);

    cJSON_AddBoolToObject(tools, "listChanged", false);
    cJSON_AddItemToObject(capabilities, "tools", tools);
    cJSON_AddItemToObject(result, "capabilities", capabilities);

    cJSON_AddItemToObject(response, "result", result);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(request);
    cJSON_Delete(response);

    ESP_LOGI(TAG, "Server initialized");
    return response_str;
}

// Handle tools/list request
static char* handle_tools_list(const char* request_data) {
    cJSON *request = cJSON_Parse(request_data);
    if (!request) {
        return create_error_response("", -32700, "Parse error");
    }

    cJSON *id_item = cJSON_GetObjectItem(request, "id");
    const char* id = id_item ? cJSON_GetStringValue(id_item) : "";

    cJSON *response = cJSON_CreateObject();
    cJSON *result = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateArray();

    // Add echo tool
    cJSON *echo_tool = cJSON_CreateObject();
    cJSON *echo_schema = cJSON_CreateObject();
    cJSON *echo_props = cJSON_CreateObject();
    cJSON *text_prop = cJSON_CreateObject();
    cJSON *echo_required = cJSON_CreateArray();

    cJSON_AddStringToObject(echo_tool, "name", "echo");
    cJSON_AddStringToObject(echo_tool, "description", "Echo back the input text");

    cJSON_AddStringToObject(echo_schema, "type", "object");
    cJSON_AddStringToObject(text_prop, "type", "string");
    cJSON_AddStringToObject(text_prop, "description", "Text to echo back");
    cJSON_AddItemToObject(echo_props, "text", text_prop);
    cJSON_AddItemToObject(echo_schema, "properties", echo_props);
    cJSON_AddItemToArray(echo_required, cJSON_CreateString("text"));
    cJSON_AddItemToObject(echo_schema, "required", echo_required);
    cJSON_AddItemToObject(echo_tool, "inputSchema", echo_schema);

    cJSON_AddItemToArray(tools, echo_tool);

    // Add GPIO tool
    cJSON *gpio_tool = cJSON_CreateObject();
    cJSON *gpio_schema = cJSON_CreateObject();
    cJSON *gpio_props = cJSON_CreateObject();
    cJSON *pin_prop = cJSON_CreateObject();
    cJSON *state_prop = cJSON_CreateObject();
    cJSON *state_enum = cJSON_CreateArray();
    cJSON *gpio_required = cJSON_CreateArray();

    cJSON_AddStringToObject(gpio_tool, "name", "gpio_control");
    cJSON_AddStringToObject(gpio_tool, "description", "Control GPIO pins on ESP8266");

    cJSON_AddStringToObject(gpio_schema, "type", "object");
    cJSON_AddStringToObject(pin_prop, "type", "integer");
    cJSON_AddStringToObject(pin_prop, "description", "GPIO pin number");
    cJSON_AddItemToObject(gpio_props, "pin", pin_prop);

    cJSON_AddStringToObject(state_prop, "type", "string");
    cJSON_AddItemToArray(state_enum, cJSON_CreateString("high"));
    cJSON_AddItemToArray(state_enum, cJSON_CreateString("low"));
    cJSON_AddItemToObject(state_prop, "enum", state_enum);
    cJSON_AddStringToObject(state_prop, "description", "GPIO state");
    cJSON_AddItemToObject(gpio_props, "state", state_prop);

    cJSON_AddItemToObject(gpio_schema, "properties", gpio_props);
    cJSON_AddItemToArray(gpio_required, cJSON_CreateString("pin"));
    cJSON_AddItemToArray(gpio_required, cJSON_CreateString("state"));
    cJSON_AddItemToObject(gpio_schema, "required", gpio_required);
    cJSON_AddItemToObject(gpio_tool, "inputSchema", gpio_schema);

    cJSON_AddItemToArray(tools, gpio_tool);

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddStringToObject(response, "id", id);
    cJSON_AddItemToObject(result, "tools", tools);
    cJSON_AddItemToObject(response, "result", result);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(request);
    cJSON_Delete(response);

    return response_str;
}

// Handle tools/call request
static char* handle_tools_call(const char* request_data) {
    cJSON *request = cJSON_Parse(request_data);
    if (!request) {
        return create_error_response("", -32700, "Parse error");
    }

    cJSON *id_item = cJSON_GetObjectItem(request, "id");
    const char* id = id_item ? cJSON_GetStringValue(id_item) : "";

    cJSON *params = cJSON_GetObjectItem(request, "params");
    if (!params) {
        cJSON_Delete(request);
        return create_error_response(id, -32602, "Invalid params");
    }

    cJSON *name_item = cJSON_GetObjectItem(params, "name");
    if (!name_item) {
        cJSON_Delete(request);
        return create_error_response(id, -32602, "Missing tool name");
    }

    const char* tool_name = cJSON_GetStringValue(name_item);
    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");

    cJSON *response = cJSON_CreateObject();
    cJSON *result = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();

    if (strcmp(tool_name, "echo") == 0) {
        if (arguments) {
            cJSON *text_item = cJSON_GetObjectItem(arguments, "text");
            if (text_item && cJSON_IsString(text_item)) {
                cJSON *text_content = cJSON_CreateObject();
                char echo_response[256];
                snprintf(echo_response, sizeof(echo_response), "Echo: %s", cJSON_GetStringValue(text_item));

                cJSON_AddStringToObject(text_content, "type", "text");
                cJSON_AddStringToObject(text_content, "text", echo_response);
                cJSON_AddItemToArray(content, text_content);

                ESP_LOGI(TAG, "Echo tool called with: %s", cJSON_GetStringValue(text_item));
            } else {
                cJSON_Delete(request);
                cJSON_Delete(response);
                return create_error_response(id, -32602, "Missing required parameter: text");
            }
        }
    } else if (strcmp(tool_name, "gpio_control") == 0) {
        if (arguments) {
            cJSON *pin_item = cJSON_GetObjectItem(arguments, "pin");
            cJSON *state_item = cJSON_GetObjectItem(arguments, "state");

            if (pin_item && cJSON_IsNumber(pin_item) && state_item && cJSON_IsString(state_item)) {
                int pin = (int)pin_item->valuedouble;
                const char* state = cJSON_GetStringValue(state_item);

                cJSON *text_content = cJSON_CreateObject();
                char gpio_response[128];
                snprintf(gpio_response, sizeof(gpio_response), "GPIO pin %d set to %s", pin, state);

                cJSON_AddStringToObject(text_content, "type", "text");
                cJSON_AddStringToObject(text_content, "text", gpio_response);
                cJSON_AddItemToArray(content, text_content);

                ESP_LOGI(TAG, "GPIO tool called: pin %d, state %s", pin, state);
            } else {
                cJSON_Delete(request);
                cJSON_Delete(response);
                return create_error_response(id, -32602, "Missing required parameters: pin, state");
            }
        }
    } else {
        cJSON_Delete(request);
        cJSON_Delete(response);
        return create_error_response(id, -32601, "Unknown tool");
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddStringToObject(response, "id", id);
    cJSON_AddItemToObject(result, "content", content);
    cJSON_AddItemToObject(response, "result", result);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(request);
    cJSON_Delete(response);

    return response_str;
}

// Handle ping request
static char* handle_ping(const char* request_data) {
    cJSON *request = cJSON_Parse(request_data);
    if (!request) {
        return create_error_response("", -32700, "Parse error");
    }

    cJSON *id_item = cJSON_GetObjectItem(request, "id");
    const char* id = id_item ? cJSON_GetStringValue(id_item) : "";

    cJSON *response = cJSON_CreateObject();
    cJSON *result = cJSON_CreateObject();

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddStringToObject(response, "id", id);
    cJSON_AddItemToObject(response, "result", result);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(request);
    cJSON_Delete(response);

    return response_str;
}

// Parse and route request
static char* process_request(const char* request_data) {
    cJSON *request = cJSON_Parse(request_data);
    if (!request) {
        return create_error_response("", -32700, "Parse error");
    }

    cJSON *jsonrpc = cJSON_GetObjectItem(request, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(cJSON_GetStringValue(jsonrpc), "2.0") != 0) {
        cJSON_Delete(request);
        return create_error_response("", -32600, "Invalid JSON-RPC version");
    }

    cJSON *method = cJSON_GetObjectItem(request, "method");
    if (!method || !cJSON_IsString(method)) {
        cJSON_Delete(request);
        return create_error_response("", -32600, "Missing method");
    }

    const char* method_name = cJSON_GetStringValue(method);
    cJSON_Delete(request);

    if (strcmp(method_name, "initialize") == 0) {
        return handle_initialize(request_data);
    } else if (strcmp(method_name, "tools/list") == 0) {
        return handle_tools_list(request_data);
    } else if (strcmp(method_name, "tools/call") == 0) {
        return handle_tools_call(request_data);
    } else if (strcmp(method_name, "ping") == 0) {
        return handle_ping(request_data);
    } else {
        return create_error_response("", -32601, "Method not found");
    }
}

// Main server loop
void simple_mcp_server_run(int client_socket) {
    char buffer[MAX_BUFFER_SIZE];
    int bytes_received;

    ESP_LOGI(TAG, "Simple MCP server started for client");

    while (1) {
        // Clear buffer
        memset(buffer, 0, sizeof(buffer));

        // Receive data
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            ESP_LOGI(TAG, "Client disconnected or error occurred");
            break;
        }

        buffer[bytes_received] = '\0';
        ESP_LOGI(TAG, "Received: %.*s", bytes_received, buffer);

        // Process request and get response
        char* response = process_request(buffer);
        if (response) {
            int response_len = strlen(response);

            // Send response
            int bytes_sent = send(client_socket, response, response_len, 0);
            if (bytes_sent < 0) {
                ESP_LOGE(TAG, "Error sending response");
                free(response);
                break;
            }

            ESP_LOGI(TAG, "Sent: %s", response);
            free(response);
        }
    }

    ESP_LOGI(TAG, "Simple MCP server stopped");
}
