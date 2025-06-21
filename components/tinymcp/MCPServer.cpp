#include "MCPServer.h"
#include "esp_log.h"
#include "lightweight_json.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sstream>

static const char *TAG = "MCPServer";

namespace tinymcp {

MCPServer::MCPServer(Transport* transport)
    : transport_(transport), running_(false), initialized_(false),
      error_count_(0), last_error_time_(0) {
    ESP_LOGI(TAG, "MCPServer created");
    
    // Test cJSON operations during initialization
    if (!tinymcp::JsonValue::testCJSONOperations()) {
        ESP_LOGE(TAG, "cJSON test failed during initialization!");
    } else {
        ESP_LOGI(TAG, "cJSON test passed during initialization");
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

    std::string buffer;
    while (running_ && transport_->isConnected()) {
        if (transport_->read(buffer)) {
            if (!buffer.empty()) {
                ESP_LOGD(TAG, "Received message (%d bytes): %s", buffer.length(), buffer.c_str());
                processMessage(buffer);
                buffer.clear();
            }
            // If buffer is empty, no complete message available yet
        } else {
            // Read failed, connection likely closed
            ESP_LOGI(TAG, "Transport read failed, stopping server");
            break;
        }

        // Small delay to prevent busy waiting
        vTaskDelay(pdMS_TO_TICKS(10));
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
    // Fast-fail: Check for empty messages
    if (message.empty() || message.find_first_not_of(" \t\r\n") == std::string::npos) {
        ESP_LOGD(TAG, "Received empty message, ignoring");
        return;
    }

    // Fast-fail: Check if this looks like an HTTP request
    if (message.find("GET ") == 0 || message.find("POST ") == 0 ||
        message.find("PUT ") == 0 || message.find("DELETE ") == 0 ||
        message.find("HEAD ") == 0 || message.find("OPTIONS ") == 0 ||
        message.find("Host:") == 0 || message.find("Connection:") == 0 ||
        message.find("User-Agent:") == 0 || message.find("Accept:") == 0 ||
        message.find("Cache-Control:") == 0 || message.find("Upgrade-Insecure-Requests:") == 0 ||
        message.find("Accept-Encoding:") == 0 || message.find("Accept-Language:") == 0) {
        ESP_LOGW(TAG, "Received HTTP request, closing connection");
        stop();  // Close the connection immediately
        return;
    }

    std::string method, id, params;

    if (!parseRequest(message, method, id, params)) {
        ESP_LOGE(TAG, "Failed to parse request: %s", message.c_str());
        std::string error = createErrorResponse("", -32700, "Parse error");
        sendResponse(error);
        return;
    }

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
    if (transport_ && !response.empty()) {
        ESP_LOGD(TAG, "Sending response: %s", response.c_str());
        if (!transport_->write(response + "\n")) {
            ESP_LOGE(TAG, "Failed to send response");
        }
    }
}

std::string MCPServer::handleInitialize(const std::string& request) {
    ESP_LOGI(TAG, "handleInitialize: Starting initialization");
    tinymcp::JsonValue root;
    tinymcp::JsonReader reader;

    if (!reader.parse(request, root)) {
        ESP_LOGE(TAG, "handleInitialize: Failed to parse request");
        return createErrorResponse("", -32700, "Parse error");
    }

    std::string id = root.get("id", tinymcp::JsonValue::createString("")).asString();
    ESP_LOGI(TAG, "handleInitialize: Request ID: %s", id.c_str());

    // Create response with server capabilities
    ESP_LOGI(TAG, "handleInitialize: Creating response object");
    tinymcp::JsonValue response = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "handleInitialize: Setting jsonrpc field");
    response.set("jsonrpc", "2.0");
    ESP_LOGI(TAG, "handleInitialize: Setting id field");
    response.set("id", id);

    ESP_LOGI(TAG, "handleInitialize: Creating result object");
    tinymcp::JsonValue result = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "handleInitialize: Setting protocolVersion");
    result.set("protocolVersion", "2024-11-05");

    ESP_LOGI(TAG, "handleInitialize: Creating serverInfo object");
    tinymcp::JsonValue serverInfo = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "handleInitialize: Setting server name");
    serverInfo.set("name", "ESP8266-MCP");
    ESP_LOGI(TAG, "handleInitialize: Setting server version");
    serverInfo.set("version", "1.0.0");
    ESP_LOGI(TAG, "handleInitialize: Adding serverInfo to result");
    result.set("serverInfo", serverInfo);

    ESP_LOGI(TAG, "handleInitialize: Creating capabilities object");
    tinymcp::JsonValue capabilities = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "handleInitialize: Creating tools capability object");
    tinymcp::JsonValue tools = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "handleInitialize: Setting tools listChanged");
    tools.set("listChanged", false);
    ESP_LOGI(TAG, "handleInitialize: Adding tools to capabilities");
    capabilities.set("tools", tools);
    ESP_LOGI(TAG, "handleInitialize: Adding capabilities to result");
    result.set("capabilities", capabilities);

    ESP_LOGI(TAG, "handleInitialize: Adding result to response");
    response.set("result", result);

    ESP_LOGI(TAG, "handleInitialize: Validating response structure");
    if (!response.isValidStructure()) {
        ESP_LOGE(TAG, "handleInitialize: Response structure validation failed!");
        return createErrorResponse(id, -32603, "Internal error - invalid response structure");
    }

    initialized_ = true;
    ESP_LOGI(TAG, "handleInitialize: Server initialized, serializing response");

    std::string serialized = response.toStringCompact();
    ESP_LOGI(TAG, "handleInitialize: Serialized response length: %d", serialized.length());
    if (serialized.empty()) {
        ESP_LOGE(TAG, "handleInitialize: Serialization failed!");
        return createErrorResponse(id, -32603, "Internal error - serialization failed");
    }
    
    ESP_LOGI(TAG, "handleInitialize: Response ready");
    return serialized;
}

std::string MCPServer::handleToolsList(const std::string& request) {
    tinymcp::JsonValue root;
    tinymcp::JsonReader reader;

    if (!reader.parse(request, root)) {
        return createErrorResponse("", -32700, "Parse error");
    }

    std::string id = root.get("id", tinymcp::JsonValue::createString("")).asString();

    tinymcp::JsonValue response = tinymcp::JsonValue::createObject();
    response.set("jsonrpc", "2.0");
    response.set("id", id);

    tinymcp::JsonValue result = tinymcp::JsonValue::createObject();
    tinymcp::JsonValue tools = tinymcp::JsonValue::createArray();

    // Add example tools
    tinymcp::JsonValue echoTool = tinymcp::JsonValue::createObject();
    echoTool.set("name", "echo");
    echoTool.set("description", "Echo back the input text");

    tinymcp::JsonValue echoSchema = tinymcp::JsonValue::createObject();
    echoSchema.set("type", "object");
    tinymcp::JsonValue echoProps = tinymcp::JsonValue::createObject();
    tinymcp::JsonValue textProp = tinymcp::JsonValue::createObject();
    textProp.set("type", "string");
    textProp.set("description", "Text to echo back");
    echoProps.set("text", textProp);
    echoSchema.set("properties", echoProps);
    tinymcp::JsonValue echoRequired = tinymcp::JsonValue::createArray();
    echoRequired.append("text");
    echoSchema.set("required", echoRequired);

    echoTool.set("inputSchema", echoSchema);
    tools.append(echoTool);

    // Add GPIO control tool
    tinymcp::JsonValue gpioTool = tinymcp::JsonValue::createObject();
    gpioTool.set("name", "gpio_control");
    gpioTool.set("description", "Control GPIO pins on ESP8266");

    tinymcp::JsonValue gpioSchema = tinymcp::JsonValue::createObject();
    gpioSchema.set("type", "object");
    tinymcp::JsonValue gpioProps = tinymcp::JsonValue::createObject();
    tinymcp::JsonValue pinProp = tinymcp::JsonValue::createObject();
    pinProp.set("type", "integer");
    pinProp.set("description", "GPIO pin number");
    gpioProps.set("pin", pinProp);
    tinymcp::JsonValue stateProp = tinymcp::JsonValue::createObject();
    stateProp.set("type", "string");
    tinymcp::JsonValue enumArray = tinymcp::JsonValue::createArray();
    enumArray.append("high");
    enumArray.append("low");
    stateProp.set("enum", enumArray);
    stateProp.set("description", "GPIO state");
    gpioProps.set("state", stateProp);
    gpioSchema.set("properties", gpioProps);
    tinymcp::JsonValue gpioRequired = tinymcp::JsonValue::createArray();
    gpioRequired.append("pin");
    gpioRequired.append("state");
    gpioSchema.set("required", gpioRequired);

    gpioTool.set("inputSchema", gpioSchema);
    tools.append(gpioTool);

    result.set("tools", tools);
    response.set("result", result);

    return response.toStringCompact();
}

std::string MCPServer::handleToolsCall(const std::string& request) {
    ESP_LOGI(TAG, "handleToolsCall: Starting to process tool call");
    tinymcp::JsonValue root;
    tinymcp::JsonReader reader;

    if (!reader.parse(request, root)) {
        ESP_LOGE(TAG, "handleToolsCall: JSON parse failed");
        return createErrorResponse("", -32700, "Parse error");
    }

    std::string id = root.get("id", tinymcp::JsonValue::createString("")).asString();
    ESP_LOGI(TAG, "handleToolsCall: Request ID: %s", id.c_str());

    tinymcp::JsonValue params = root.get("params", tinymcp::JsonValue());

    if (!params.isObject()) {
        ESP_LOGE(TAG, "handleToolsCall: Invalid params - not an object");
        return createErrorResponse(id, -32602, "Invalid params");
    }

    std::string toolName = params.get("name", tinymcp::JsonValue::createString("")).asString();
    ESP_LOGI(TAG, "handleToolsCall: Tool name: %s", toolName.c_str());

    tinymcp::JsonValue arguments = params.get("arguments", tinymcp::JsonValue());

    tinymcp::JsonValue response = tinymcp::JsonValue::createObject();
    response.set("jsonrpc", "2.0");
    response.set("id", id);

    tinymcp::JsonValue result = tinymcp::JsonValue::createObject();
    tinymcp::JsonValue content = tinymcp::JsonValue::createArray();

    if (toolName == "echo") {
        ESP_LOGI(TAG, "handleToolsCall: Processing echo tool");
        if (arguments.isMember("text")) {
            ESP_LOGI(TAG, "handleToolsCall: Echo has text parameter");
            tinymcp::JsonValue textContent = tinymcp::JsonValue::createObject();
            textContent.set("type", "text");
            textContent.set("text", "Echo: " + arguments.get("text").asString());
            content.append(textContent);
            ESP_LOGI(TAG, "Echo tool called with: %s", arguments.get("text").asString().c_str());
        } else {
            ESP_LOGI(TAG, "handleToolsCall: Echo missing text parameter, returning error");
            return createErrorResponse(id, -32602, "Missing required parameter: text");
        }
    } else if (toolName == "gpio_control") {
        if (arguments.isMember("pin") && arguments.isMember("state")) {
            int pin = arguments.get("pin").asInt();
            std::string state = arguments.get("state").asString();

            // Simple GPIO control simulation
            tinymcp::JsonValue textContent = tinymcp::JsonValue::createObject();
            textContent.set("type", "text");
            textContent.set("text", "GPIO pin " + std::to_string(pin) + " set to " + state);
            content.append(textContent);

            ESP_LOGI(TAG, "GPIO tool called: pin %d, state %s", pin, state.c_str());
        } else {
            return createErrorResponse(id, -32602, "Missing required parameters: pin, state");
        }
    } else {
        ESP_LOGI(TAG, "handleToolsCall: Unknown tool '%s', returning error", toolName.c_str());
        return createErrorResponse(id, -32601, "Unknown tool: " + toolName);
    }

    ESP_LOGI(TAG, "handleToolsCall: Setting result and returning success response");
    result.set("content", content);
    response.set("result", result);

    return response.toStringCompact();
}

std::string MCPServer::handlePing(const std::string& request) {
    tinymcp::JsonValue root;
    tinymcp::JsonReader reader;

    if (!reader.parse(request, root)) {
        return createErrorResponse("", -32700, "Parse error");
    }

    std::string id = root.get("id", tinymcp::JsonValue::createString("")).asString();

    tinymcp::JsonValue response = tinymcp::JsonValue::createObject();
    response.set("jsonrpc", "2.0");
    response.set("id", id);
    response.set("result", tinymcp::JsonValue::createObject());

    return response.toStringCompact();
}

std::string MCPServer::createErrorResponse(const std::string& id, int code, const std::string& message) {
    ESP_LOGI(TAG, "createErrorResponse: Creating error response - id: %s, code: %d, message: %s", id.c_str(), code, message.c_str());

    ESP_LOGI(TAG, "createErrorResponse: Creating root object");
    tinymcp::JsonValue response = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "createErrorResponse: Setting jsonrpc field");
    response.set("jsonrpc", "2.0");
    ESP_LOGI(TAG, "createErrorResponse: Setting id field");
    if (id.empty()) {
        response.set("id", tinymcp::JsonValue::createNull());
    } else {
        response.set("id", id);
    }

    ESP_LOGI(TAG, "createErrorResponse: Creating error object");
    tinymcp::JsonValue error = tinymcp::JsonValue::createObject();
    ESP_LOGI(TAG, "createErrorResponse: Setting error code: %d", code);
    error.set("code", code);
    ESP_LOGI(TAG, "createErrorResponse: Setting error message: %s", message.c_str());
    error.set("message", message);
    ESP_LOGI(TAG, "createErrorResponse: Setting error object on response");
    response.set("error", error);

    ESP_LOGI(TAG, "createErrorResponse: About to serialize response");
    std::string result = response.toStringCompact();
    ESP_LOGI(TAG, "createErrorResponse: Serialized response length: %d", result.length());
    ESP_LOGI(TAG, "createErrorResponse: Serialized response: %s", result.c_str());

    if (result.empty()) {
        ESP_LOGE(TAG, "createErrorResponse: JSON serialization failed! Returning manual JSON");
        std::string manual_json = "{\"jsonrpc\":\"2.0\",\"id\":\"" + id + "\",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":\"" + message + "\"}}";
        ESP_LOGI(TAG, "createErrorResponse: Manual JSON: %s", manual_json.c_str());
        return manual_json;
    }

    return result;
}

std::string MCPServer::createSuccessResponse(const std::string& id, const std::string& result) {
    tinymcp::JsonValue response = tinymcp::JsonValue::createObject();
    response.set("jsonrpc", "2.0");
    response.set("id", id);
    response.set("result", result);

    return response.toStringCompact();
}

bool MCPServer::parseRequest(const std::string& message, std::string& method, std::string& id, std::string& params) {
    tinymcp::JsonValue root;
    tinymcp::JsonReader reader;

    ESP_LOGD(TAG, "Parsing message: %s", message.c_str());

    if (!reader.parse(message, root)) {
        ESP_LOGE(TAG, "JSON parse error for message: %s", message.c_str());
        return false;
    }

    if (!root.isMember("jsonrpc") || root.get("jsonrpc").asString() != "2.0") {
        ESP_LOGE(TAG, "Invalid JSON-RPC version in message: %s", message.c_str());
        return false;
    }

    if (!root.isMember("method")) {
        ESP_LOGE(TAG, "Missing method in request: %s", message.c_str());
        return false;
    }

    method = root.get("method").asString();
    id = root.get("id", tinymcp::JsonValue::createString("")).asString();

    if (root.isMember("params")) {
        params = root.get("params").toStringCompact();
    }

    ESP_LOGD(TAG, "Successfully parsed: method=%s, id=%s", method.c_str(), id.c_str());
    return true;
}

} // namespace tinymcp
