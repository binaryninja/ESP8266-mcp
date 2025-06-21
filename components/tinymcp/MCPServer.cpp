#include "MCPServer.h"
#include "esp_log.h"
#include "simple_json.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <sstream>
#include <cstring>
#include "cJSON.h"

static const char *TAG = "MCPServer";

// JSON processing task structure
typedef struct {
    json_operation_type_t operation;
    std::string input;
    std::string output;
    bool success;
    std::string error_msg;
    SemaphoreHandle_t completion_sem;
    
    // Operation-specific parameters
    std::string param1;  // For error code, tool name, etc.
    std::string param2;  // For error message, tool args, etc.
    int param_int;       // For error codes
} json_task_data_t;

static TaskHandle_t json_task_handle = NULL;
static QueueHandle_t json_queue = NULL;

namespace tinymcp {

MCPServer::MCPServer(Transport* transport)
    : transport_(transport), running_(false), initialized_(false),
      error_count_(0), last_error_time_(0) {
    ESP_LOGI(TAG, "MCPServer created");
    
    // Test basic JSON operations during initialization
    ESP_LOGI(TAG, "MCPServer: Testing basic JSON creation...");
    // Very simple test - just create a null value with cJSON
    cJSON* test = cJSON_CreateNull();
    if (test != NULL) {
        ESP_LOGI(TAG, "MCPServer: Basic JSON null value created successfully");
        cJSON_Delete(test);
        
        // Test creating an object
        cJSON* obj = cJSON_CreateObject();
        if (obj != NULL) {
            ESP_LOGI(TAG, "MCPServer: JSON object created successfully");
            cJSON_Delete(obj);
        }
        
        ESP_LOGI(TAG, "MCPServer: JSON test passed during initialization");
    } else {
        ESP_LOGE(TAG, "MCPServer: JSON test failed during initialization");
    }
}

MCPServer::~MCPServer() {
    stop();
    ESP_LOGI(TAG, "MCPServer destroyed");
}

void MCPServer::run() {
    if (!transport_) {
        ESP_LOGE(TAG, "Transport is null");
        return;
    }

    running_ = true;
    ESP_LOGI(TAG, "MCP Server starting...");
    
    ESP_LOGI(TAG, "Debug: About to create std::string buffer");
    std::string buffer;
    ESP_LOGI(TAG, "Debug: Buffer created successfully");
    
    ESP_LOGI(TAG, "Debug: About to enter main loop");
    while (running_) {
        ESP_LOGI(TAG, "Debug: Loop iteration start");
        
        ESP_LOGI(TAG, "Debug: About to call isConnected()");
        bool connected = transport_->isConnected();
        ESP_LOGI(TAG, "Debug: isConnected() returned: %s", connected ? "true" : "false");
        
        if (!connected) {
            ESP_LOGI(TAG, "Debug: Transport not connected, breaking");
            break;
        }
        
        ESP_LOGI(TAG, "Debug: About to call transport->read()");
        bool read_result = transport_->read(buffer);
        ESP_LOGI(TAG, "Debug: transport->read() returned: %s", read_result ? "true" : "false");
        
        if (read_result) {
            ESP_LOGI(TAG, "Debug: Read successful, buffer size: %d", buffer.size());
            if (!buffer.empty()) {
                ESP_LOGD(TAG, "Received message (%d bytes): %s", buffer.length(), buffer.c_str());
                ESP_LOGI(TAG, "Debug: About to process message");
                processMessage(buffer);
                ESP_LOGI(TAG, "Debug: Message processed, clearing buffer");
                buffer.clear();
                ESP_LOGI(TAG, "Debug: Buffer cleared");
            }
            // If buffer is empty, no complete message available yet
        } else {
            // Read failed, connection likely closed
            ESP_LOGI(TAG, "Transport read failed, stopping server");
            break;
        }

        ESP_LOGI(TAG, "Debug: About to delay");
        // Small delay to prevent busy waiting
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "Debug: Delay completed");
    }

    running_ = false;
    ESP_LOGI(TAG, "MCP Server stopped");
}

void MCPServer::stop() {
    running_ = false;
    if (transport_) {
        transport_->close();
    }
}

bool MCPServer::isRunning() const {
    return running_;
}

void MCPServer::processMessage(const std::string& message) {
    ESP_LOGI(TAG, "Debug: processMessage() entered");
    
    // Use safer C-style string handling initially
    const char* msg_ptr = message.c_str();
    if (!msg_ptr) {
        ESP_LOGE(TAG, "Debug: String pointer is null!");
        return;
    }
    
    // Use strlen instead of std::string::size() to avoid potential issues
    size_t msg_len = strlen(msg_ptr);
    ESP_LOGI(TAG, "Debug: Message length: %d", (int)msg_len);
    
    if (msg_len == 0) {
        ESP_LOGD(TAG, "Received empty message, ignoring");
        return;
    }
    
    // Simple HTTP detection using C string functions
    if (strncmp(msg_ptr, "GET ", 4) == 0 || 
        strncmp(msg_ptr, "POST ", 5) == 0 ||
        strncmp(msg_ptr, "PUT ", 4) == 0 ||
        strncmp(msg_ptr, "DELETE ", 7) == 0 ||
        strstr(msg_ptr, "User-Agent:") != NULL ||
        strstr(msg_ptr, "Host:") != NULL) {
        ESP_LOGW(TAG, "Received HTTP request, closing connection");
        stop();
        return;
    }
    
    ESP_LOGI(TAG, "Debug: About to create method/id/params strings");
    std::string method, id, params;
    ESP_LOGI(TAG, "Debug: Calling parseRequest");

    if (!parseRequest(message, method, id, params)) {
        ESP_LOGE(TAG, "Failed to parse request");
        ESP_LOGI(TAG, "Debug: Parse failed, creating error response");
        std::string error = createErrorResponse("", -32700, "Parse error");
        ESP_LOGI(TAG, "Debug: Sending error response");
        sendResponse(error);
        return;
    }

    ESP_LOGI(TAG, "Debug: parseRequest succeeded");
    ESP_LOGD(TAG, "Processing method: %s, id: %s", method.c_str(), id.c_str());

    std::string response;

    if (method == "initialize") {
        response = handleInitialize(message);
    } else if (method == "tools/list") {
        if (!initialized_) {
            response = createErrorResponse(id, -32002, "Server not initialized");
        } else {
            response = handleToolsList(message);
        }
    } else if (method == "tools/call") {
        if (!initialized_) {
            response = createErrorResponse(id, -32002, "Server not initialized");
        } else {
            response = handleToolsCall(message);
        }
    } else if (method == "ping") {
        response = handlePing(message);
    } else {
        response = createErrorResponse(id, -32601, "Method not found");
    }

    sendResponse(response);
}

void MCPServer::sendResponse(const std::string& response) {
    ESP_LOGI(TAG, "sendResponse: Starting response transmission");
    ESP_LOGI(TAG, "sendResponse: Response length: %d", response.length());
    ESP_LOGI(TAG, "sendResponse: Transport pointer: %p", transport_);
    ESP_LOGI(TAG, "sendResponse: Is running: %s", running_ ? "true" : "false");
    
    if (!transport_) {
        ESP_LOGE(TAG, "sendResponse: Transport is null - CANNOT SEND");
        return;
    }
    
    if (response.empty()) {
        ESP_LOGW(TAG, "sendResponse: Response is empty - CANNOT SEND");
        return;
    }
    
    if (!running_) {
        ESP_LOGW(TAG, "sendResponse: Server not running - CANNOT SEND");
        return;
    }
    
    // Log first 200 characters of response for debugging
    std::string responsePreview = response.substr(0, 200);
    if (response.length() > 200) responsePreview += "...";
    ESP_LOGI(TAG, "sendResponse: Response content: %s", responsePreview.c_str());
    
    ESP_LOGI(TAG, "sendResponse: About to call transport->write");
    ESP_LOGI(TAG, "sendResponse: Stack remaining: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    
    // Create a copy of the response with newline to avoid potential string corruption
    std::string responseToSend = response + "\n";
    ESP_LOGI(TAG, "sendResponse: Response with newline length: %d", responseToSend.length());
    
    ESP_LOGI(TAG, "sendResponse: Calling transport->write...");
    bool writeResult = transport_->write(responseToSend);
    ESP_LOGI(TAG, "sendResponse: Transport write returned: %s", writeResult ? "true" : "false");
    
    if (!writeResult) {
        ESP_LOGE(TAG, "sendResponse: FAILED to send response - transport write failed");
    } else {
        ESP_LOGI(TAG, "sendResponse: Response sent successfully");
    }
    
    ESP_LOGI(TAG, "sendResponse: Function complete");
}

// JSON processing task with larger stack
static void json_processing_task(void* pvParameters) {
    json_task_data_t* task_data;
    
    while (true) {
        // Wait for JSON processing request
        if (xQueueReceive(json_queue, &task_data, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "JSON task: Processing operation type %d", task_data->operation);
            
            cJSON* response = NULL;
            std::string id = "";
            
            // Parse input to extract ID when needed
            if (!task_data->input.empty()) {
                cJSON* root = cJSON_Parse(task_data->input.c_str());
                if (root != NULL) {
                    cJSON* id_json = cJSON_GetObjectItem(root, "id");
                    if (cJSON_IsString(id_json)) {
                        id = id_json->valuestring;
                    } else if (cJSON_IsNumber(id_json)) {
                        id = std::to_string((int)id_json->valuedouble);
                    }
                    cJSON_Delete(root);
                }
            }
            
            // Handle different operation types
            switch (task_data->operation) {
                case JSON_OP_INITIALIZE: {
                    response = cJSON_CreateObject();
                    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                    cJSON_AddStringToObject(response, "id", id.c_str());
                    
                    cJSON* result = cJSON_CreateObject();
                    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
                    
                    cJSON* serverInfo = cJSON_CreateObject();
                    cJSON_AddStringToObject(serverInfo, "name", "ESP8266-MCP");
                    cJSON_AddStringToObject(serverInfo, "version", "1.0.0");
                    cJSON_AddItemToObject(result, "serverInfo", serverInfo);
                    
                    cJSON* capabilities = cJSON_CreateObject();
                    cJSON* tools = cJSON_CreateObject();
                    cJSON_AddBoolToObject(tools, "listChanged", false);
                    cJSON_AddItemToObject(capabilities, "tools", tools);
                    cJSON_AddItemToObject(result, "capabilities", capabilities);
                    
                    cJSON_AddItemToObject(response, "result", result);
                    break;
                }
                
                case JSON_OP_TOOLS_LIST: {
                    response = cJSON_CreateObject();
                    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                    cJSON_AddStringToObject(response, "id", id.c_str());
                    
                    cJSON* result = cJSON_CreateObject();
                    cJSON* tools = cJSON_CreateArray();
                    
                    // Add echo tool
                    cJSON* echoTool = cJSON_CreateObject();
                    cJSON_AddStringToObject(echoTool, "name", "echo");
                    cJSON_AddStringToObject(echoTool, "description", "Echo back the input text");
                    
                    cJSON* echoSchema = cJSON_CreateObject();
                    cJSON_AddStringToObject(echoSchema, "type", "object");
                    cJSON* echoProps = cJSON_CreateObject();
                    cJSON* textProp = cJSON_CreateObject();
                    cJSON_AddStringToObject(textProp, "type", "string");
                    cJSON_AddStringToObject(textProp, "description", "Text to echo back");
                    cJSON_AddItemToObject(echoProps, "text", textProp);
                    cJSON_AddItemToObject(echoSchema, "properties", echoProps);
                    cJSON* echoRequired = cJSON_CreateArray();
                    cJSON_AddItemToArray(echoRequired, cJSON_CreateString("text"));
                    cJSON_AddItemToObject(echoSchema, "required", echoRequired);
                    cJSON_AddItemToObject(echoTool, "inputSchema", echoSchema);
                    cJSON_AddItemToArray(tools, echoTool);
                    
                    // Add GPIO tool
                    cJSON* gpioTool = cJSON_CreateObject();
                    cJSON_AddStringToObject(gpioTool, "name", "gpio_control");
                    cJSON_AddStringToObject(gpioTool, "description", "Control GPIO pins on ESP8266");
                    
                    cJSON* gpioSchema = cJSON_CreateObject();
                    cJSON_AddStringToObject(gpioSchema, "type", "object");
                    cJSON* gpioProps = cJSON_CreateObject();
                    cJSON* pinProp = cJSON_CreateObject();
                    cJSON_AddStringToObject(pinProp, "type", "integer");
                    cJSON_AddStringToObject(pinProp, "description", "GPIO pin number");
                    cJSON_AddItemToObject(gpioProps, "pin", pinProp);
                    cJSON* stateProp = cJSON_CreateObject();
                    cJSON_AddStringToObject(stateProp, "type", "string");
                    cJSON* enumArray = cJSON_CreateArray();
                    cJSON_AddItemToArray(enumArray, cJSON_CreateString("high"));
                    cJSON_AddItemToArray(enumArray, cJSON_CreateString("low"));
                    cJSON_AddItemToObject(stateProp, "enum", enumArray);
                    cJSON_AddStringToObject(stateProp, "description", "Pin state (high or low)");
                    cJSON_AddItemToObject(gpioProps, "state", stateProp);
                    cJSON_AddItemToObject(gpioSchema, "properties", gpioProps);
                    cJSON* gpioRequired = cJSON_CreateArray();
                    cJSON_AddItemToArray(gpioRequired, cJSON_CreateString("pin"));
                    cJSON_AddItemToArray(gpioRequired, cJSON_CreateString("state"));
                    cJSON_AddItemToObject(gpioSchema, "required", gpioRequired);
                    cJSON_AddItemToObject(gpioTool, "inputSchema", gpioSchema);
                    cJSON_AddItemToArray(tools, gpioTool);
                    
                    cJSON_AddItemToObject(result, "tools", tools);
                    cJSON_AddItemToObject(response, "result", result);
                    break;
                }
                
                case JSON_OP_TOOLS_CALL: {
                    // Extract tool name and arguments from param1 and param2
                    std::string toolName = task_data->param1;
                    std::string argumentsJson = task_data->param2;
                    
                    ESP_LOGI(TAG, "JSON Task: Processing tool call for '%s'", toolName.c_str());
                    
                    // Check if tool exists - create error response for unknown tools
                    if (toolName != "echo" && toolName != "gpio_control") {
                        ESP_LOGE(TAG, "JSON Task: Unknown tool: %s", toolName.c_str());
                        ESP_LOGI(TAG, "JSON Task: Creating error response object...");
                        response = cJSON_CreateObject();
                        if (response == NULL) {
                            ESP_LOGE(TAG, "JSON Task: Failed to create response object!");
                            task_data->success = false;
                            task_data->error_msg = "Failed to create error response";
                            response = NULL;
                            break;
                        }
                        cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                        cJSON_AddStringToObject(response, "id", id.c_str());
                        
                        cJSON* error = cJSON_CreateObject();
                        if (error == NULL) {
                            ESP_LOGE(TAG, "JSON Task: Failed to create error object!");
                            cJSON_Delete(response);
                            task_data->success = false;
                            task_data->error_msg = "Failed to create error object";
                            response = NULL;
                            break;
                        }
                        cJSON_AddNumberToObject(error, "code", -32601);
                        std::string errorMsg = "Unknown tool: " + toolName;
                        cJSON_AddStringToObject(error, "message", errorMsg.c_str());
                        cJSON_AddItemToObject(response, "error", error);
                        ESP_LOGI(TAG, "JSON Task: Error response created for unknown tool - breaking");
                        break;
                    }
                    
                    // Create success response structure
                    response = cJSON_CreateObject();
                    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                    cJSON_AddStringToObject(response, "id", id.c_str());
                    
                    cJSON* result = cJSON_CreateObject();
                    cJSON* content = cJSON_CreateArray();
                    
                    // Tool-specific processing with validation
                    if (toolName == "echo") {
                        ESP_LOGI(TAG, "JSON Task: Processing echo tool");
                        cJSON* args = cJSON_Parse(argumentsJson.c_str());
                        if (args != NULL) {
                            cJSON* textItem = cJSON_GetObjectItem(args, "text");
                            if (textItem != NULL && cJSON_IsString(textItem)) {
                                cJSON* textContent = cJSON_CreateObject();
                                cJSON_AddStringToObject(textContent, "type", "text");
                                std::string echoText = "Echo: " + std::string(cJSON_GetStringValue(textItem));
                                cJSON_AddStringToObject(textContent, "text", echoText.c_str());
                                cJSON_AddItemToArray(content, textContent);
                                ESP_LOGI(TAG, "JSON Task: Echo successful with text: %s", cJSON_GetStringValue(textItem));
                            } else {
                                // Missing or invalid text parameter
                                ESP_LOGE(TAG, "JSON Task: Echo missing text parameter");
                                ESP_LOGI(TAG, "JSON Task: Cleaning up and creating error response...");
                                cJSON_Delete(args);
                                cJSON_Delete(content);
                                cJSON_Delete(result);
                                cJSON_Delete(response);
                                
                                response = cJSON_CreateObject();
                                if (response == NULL) {
                                    ESP_LOGE(TAG, "JSON Task: Failed to create echo error response!");
                                    task_data->success = false;
                                    task_data->error_msg = "Failed to create echo error response";
                                    response = NULL;
                                    break;
                                }
                                cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                                cJSON_AddStringToObject(response, "id", id.c_str());
                                
                                cJSON* error = cJSON_CreateObject();
                                if (error == NULL) {
                                    ESP_LOGE(TAG, "JSON Task: Failed to create echo error object!");
                                    cJSON_Delete(response);
                                    task_data->success = false;
                                    task_data->error_msg = "Failed to create echo error object";
                                    response = NULL;
                                    break;
                                }
                                cJSON_AddNumberToObject(error, "code", -32602);
                                cJSON_AddStringToObject(error, "message", "Missing required parameter: text");
                                cJSON_AddItemToObject(response, "error", error);
                                ESP_LOGI(TAG, "JSON Task: Error response created for invalid echo params - breaking");
                                break;
                            }
                            cJSON_Delete(args);
                        } else {
                            // Failed to parse arguments
                            ESP_LOGE(TAG, "JSON Task: Failed to parse echo arguments");
                            cJSON_Delete(content);
                            cJSON_Delete(result);
                            cJSON_Delete(response);
                            
                            response = cJSON_CreateObject();
                            cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                            cJSON_AddStringToObject(response, "id", id.c_str());
                            
                            cJSON* error = cJSON_CreateObject();
                            cJSON_AddNumberToObject(error, "code", -32602);
                            cJSON_AddStringToObject(error, "message", "Invalid arguments format");
                            cJSON_AddItemToObject(response, "error", error);
                            ESP_LOGI(TAG, "JSON Task: Error response created for invalid echo args");
                            break;
                        }
                    } else if (toolName == "gpio_control") {
                        ESP_LOGI(TAG, "JSON Task: Processing GPIO control tool");
                        cJSON* args = cJSON_Parse(argumentsJson.c_str());
                        if (args != NULL) {
                            cJSON* pinItem = cJSON_GetObjectItem(args, "pin");
                            cJSON* stateItem = cJSON_GetObjectItem(args, "state");
                            if (pinItem != NULL && cJSON_IsNumber(pinItem) && 
                                stateItem != NULL && cJSON_IsString(stateItem)) {
                                int pin = (int)pinItem->valuedouble;
                                std::string state = cJSON_GetStringValue(stateItem);
                                
                                cJSON* textContent = cJSON_CreateObject();
                                cJSON_AddStringToObject(textContent, "type", "text");
                                std::string gpioText = "GPIO pin " + std::to_string(pin) + " set to " + state;
                                cJSON_AddStringToObject(textContent, "text", gpioText.c_str());
                                cJSON_AddItemToArray(content, textContent);
                                ESP_LOGI(TAG, "JSON Task: GPIO successful: pin %d, state %s", pin, state.c_str());
                            }
                            cJSON_Delete(args);
                        }
                        ESP_LOGI(TAG, "JSON Task: GPIO processing completed");
                    }
                    
                    // Add content to result (content array should always be present)
                    cJSON_AddItemToObject(result, "content", content);
                    cJSON_AddItemToObject(response, "result", result);
                    
                    ESP_LOGI(TAG, "JSON Task: Tool call processing completed");
                    break;
                }
                
                case JSON_OP_ERROR_RESPONSE: {
                    response = cJSON_CreateObject();
                    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                    if (!task_data->param1.empty()) {
                        cJSON_AddStringToObject(response, "id", task_data->param1.c_str());
                    } else {
                        cJSON_AddNullToObject(response, "id");
                    }
                    
                    cJSON* error = cJSON_CreateObject();
                    cJSON_AddNumberToObject(error, "code", task_data->param_int);
                    cJSON_AddStringToObject(error, "message", task_data->param2.c_str());
                    cJSON_AddItemToObject(response, "error", error);
                    break;
                }
                
                case JSON_OP_PING: {
                    response = cJSON_CreateObject();
                    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
                    cJSON_AddStringToObject(response, "id", id.c_str());
                    cJSON* result = cJSON_CreateObject();
                    cJSON_AddItemToObject(response, "result", result);
                    break;
                }
                
                default:
                    task_data->success = false;
                    task_data->error_msg = "Unsupported operation";
                    response = NULL;
                    break;
            }
            
            // Convert to string using compact format
            ESP_LOGI(TAG, "JSON Task: Converting response to string...");
            if (response != NULL) {
                ESP_LOGI(TAG, "JSON Task: Response object exists, serializing...");
                char* response_str = cJSON_PrintUnformatted(response);
                if (response_str != NULL) {
                    ESP_LOGI(TAG, "JSON Task: Serialization successful, length: %d", strlen(response_str));
                    task_data->output = response_str;
                    task_data->success = true;
                    free(response_str);
                    ESP_LOGI(TAG, "JSON Task: Response assigned to task_data");
                } else {
                    ESP_LOGE(TAG, "JSON Task: Failed to serialize response!");
                    task_data->success = false;
                    task_data->error_msg = "Failed to serialize response";
                }
                cJSON_Delete(response);
                ESP_LOGI(TAG, "JSON Task: Response object deleted");
            } else {
                ESP_LOGE(TAG, "JSON Task: Response object is NULL!");
                task_data->success = false;
                task_data->error_msg = "Failed to create response";
            }
            
            ESP_LOGI(TAG, "JSON Task: Giving semaphore, success=%s", task_data->success ? "true" : "false");
            xSemaphoreGive(task_data->completion_sem);
        }
    }
}

// Helper function to process JSON operations
std::string MCPServer::processJsonOperation(json_operation_type_t operation, const std::string& input, 
                                          const std::string& param1, const std::string& param2, int param_int) {
    // Create JSON processing task if not exists
    if (json_task_handle == NULL) {
        json_queue = xQueueCreate(1, sizeof(json_task_data_t*));
        if (json_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create JSON queue");
            return createErrorResponse("", -32603, "Internal error");
        }
        
        xTaskCreatePinnedToCore(
            json_processing_task,
            "json_proc",
            8192,  // 8KB stack for JSON processing
            NULL,
            5,
            &json_task_handle,
            0
        );
        
        if (json_task_handle == NULL) {
            ESP_LOGE(TAG, "Failed to create JSON processing task");
            return createErrorResponse("", -32603, "Internal error");
        }
    }
    
    // Prepare task data
    json_task_data_t task_data;
    task_data.operation = operation;
    task_data.input = input;
    task_data.param1 = param1;
    task_data.param2 = param2;
    task_data.param_int = param_int;
    task_data.completion_sem = xSemaphoreCreateBinary();
    if (task_data.completion_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create completion semaphore");
        return createErrorResponse("", -32603, "Internal error");
    }
    
    // Send to JSON processing task
    json_task_data_t* task_data_ptr = &task_data;
    if (xQueueSend(json_queue, &task_data_ptr, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send to JSON processing task");
        vSemaphoreDelete(task_data.completion_sem);
        return createErrorResponse("", -32603, "Internal error");
    }
    
    // Wait for completion
    if (xSemaphoreTake(task_data.completion_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "JSON processing task timeout");
        vSemaphoreDelete(task_data.completion_sem);
        return createErrorResponse("", -32603, "Internal error");
    }
    
    vSemaphoreDelete(task_data.completion_sem);
    
    if (!task_data.success) {
        ESP_LOGE(TAG, "JSON processing failed: %s", task_data.error_msg.c_str());
        return createErrorResponse("", -32700, "Parse error");
    }
    
    return task_data.output;
}

std::string MCPServer::handleInitialize(const std::string& request) {
    ESP_LOGI(TAG, "handleInitialize: Starting initialization");
    
    std::string response = processJsonOperation(JSON_OP_INITIALIZE, request, "", "", 0);
    
    initialized_ = true;
    ESP_LOGI(TAG, "handleInitialize: Server initialized");
    ESP_LOGI(TAG, "handleInitialize: Response length: %d", response.length());
    return response;
}

std::string MCPServer::handleToolsList(const std::string& request) {
    return processJsonOperation(JSON_OP_TOOLS_LIST, request, "", "", 0);
}

std::string MCPServer::handleToolsCall(const std::string& request) {
    ESP_LOGI(TAG, "handleToolsCall: Starting to process tool call");
    
    // Use cJSON for lightweight parsing to extract tool name and arguments
    cJSON* root = cJSON_Parse(request.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "handleToolsCall: JSON parse failed");
        return createErrorResponse("", -32700, "Parse error");
    }

    cJSON* idItem = cJSON_GetObjectItem(root, "id");
    std::string id = (idItem && cJSON_IsString(idItem)) ? cJSON_GetStringValue(idItem) : "";
    ESP_LOGI(TAG, "handleToolsCall: Request ID: %s", id.c_str());

    cJSON* paramsItem = cJSON_GetObjectItem(root, "params");
    if (!paramsItem || !cJSON_IsObject(paramsItem)) {
        ESP_LOGE(TAG, "handleToolsCall: params is not an object");
        cJSON_Delete(root);
        return createErrorResponse(id, -32602, "Invalid params");
    }

    cJSON* nameItem = cJSON_GetObjectItem(paramsItem, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        ESP_LOGE(TAG, "handleToolsCall: missing tool name");
        cJSON_Delete(root);
        return createErrorResponse(id, -32602, "Missing tool name");
    }
    
    std::string toolName = cJSON_GetStringValue(nameItem);
    ESP_LOGI(TAG, "handleToolsCall: Tool name: %s", toolName.c_str());

    cJSON* argumentsItem = cJSON_GetObjectItem(paramsItem, "arguments");
    std::string argumentsJson = "{}";
    if (argumentsItem) {
        char* args_str = cJSON_PrintUnformatted(argumentsItem);
        if (args_str) {
            argumentsJson = args_str;
            free(args_str);
        }
    }

    ESP_LOGI(TAG, "handleToolsCall: Tool name: %s", toolName.c_str());
    ESP_LOGI(TAG, "handleToolsCall: Processing tool call (validation will be done in JSON task)");

    cJSON_Delete(root);

    ESP_LOGI(TAG, "handleToolsCall: Routing to JSON processing task");
    
    // Use the JSON processing task to generate the response
    return processJsonOperation(JSON_OP_TOOLS_CALL, request, toolName, argumentsJson, 0);
}

std::string MCPServer::handlePing(const std::string& request) {
    return processJsonOperation(JSON_OP_PING, request, "", "", 0);
}

std::string MCPServer::createErrorResponse(const std::string& id, int code, const std::string& message) {
    ESP_LOGI(TAG, "createErrorResponse: Creating error response - id: %s, code: %d, message: %s", id.c_str(), code, message.c_str());

    // Use simple cJSON for error responses to avoid recursion
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    if (!id.empty()) {
        cJSON_AddStringToObject(response, "id", id.c_str());
    } else {
        cJSON_AddNullToObject(response, "id");
    }
    
    cJSON* error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message.c_str());
    cJSON_AddItemToObject(response, "error", error);
    
    char* response_str = cJSON_PrintUnformatted(response);
    std::string responseStr = response_str ? response_str : "";
    if (response_str) free(response_str);
    cJSON_Delete(response);
    
    ESP_LOGI(TAG, "createErrorResponse: Response length: %d", responseStr.length());
    return responseStr;
}

std::string MCPServer::createSuccessResponse(const std::string& id, const std::string& result) {
    Json::Value response(Json::Value::objectValue);
    response.setMember("jsonrpc", Json::Value("2.0"));
    response.setMember("id", Json::Value(id));
    response.setMember("result", Json::Value(result));

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"];
    std::unique_ptr<Json::StreamWriterBuilder::StreamWriter> writer(writerBuilder.newStreamWriter());
    std::ostringstream oss;
    writer->write(response, &oss);
    std::string responseStr = oss.str();
    if (!responseStr.empty() && responseStr.back() == '\n') {
        responseStr.pop_back();
    }
    return responseStr;
}

bool MCPServer::parseRequest(const std::string& message, std::string& method, std::string& id, std::string& params) {
    ESP_LOGD(TAG, "Parsing JSON message with cJSON (lightweight)");
    
    // Use cJSON for lightweight parsing
    cJSON* root = cJSON_Parse(message.c_str());
    if (root == NULL) {
        const char* error_ptr = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "cJSON parse error: %s", error_ptr ? error_ptr : "unknown error");
        return false;
    }

    // Check JSON-RPC version
    cJSON* jsonrpc = cJSON_GetObjectItem(root, "jsonrpc");
    if (!cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSON-RPC version");
        cJSON_Delete(root);
        return false;
    }

    // Get method (required)
    cJSON* method_json = cJSON_GetObjectItem(root, "method");
    if (!cJSON_IsString(method_json)) {
        ESP_LOGE(TAG, "Missing or invalid method in request");
        cJSON_Delete(root);
        return false;
    }
    method = method_json->valuestring;

    // Get id (optional)
    cJSON* id_json = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id_json)) {
        id = id_json->valuestring;
    } else if (cJSON_IsNumber(id_json)) {
        id = std::to_string((int)id_json->valuedouble);
    } else {
        id = "";
    }

    // Get params (optional)
    cJSON* params_json = cJSON_GetObjectItem(root, "params");
    if (params_json != NULL) {
        char* params_str = cJSON_Print(params_json);
        if (params_str != NULL) {
            params = params_str;
            free(params_str);  // cJSON_Print uses malloc
        } else {
            params = "";
        }
    } else {
        params = "";
    }

    cJSON_Delete(root);
    ESP_LOGD(TAG, "Successfully parsed with cJSON: method=%s, id=%s", method.c_str(), id.c_str());
    return true;
}

} // namespace tinymcp
