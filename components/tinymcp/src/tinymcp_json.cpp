// JSON Serialization and Parsing Helpers for TinyMCP
// Implementation of lightweight JSON utilities for ESP32/ESP8266

#include "tinymcp_json.h"
#include <algorithm>
#include <cstring>

namespace tinymcp {

// JsonHelper implementation

bool JsonHelper::hasField(const cJSON* json, const char* key) {
    return json && cJSON_HasObjectItem(json, key);
}

bool JsonHelper::isString(const cJSON* json, const char* key) {
    if (!json) return false;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return item && cJSON_IsString(item);
}

bool JsonHelper::isNumber(const cJSON* json, const char* key) {
    if (!json) return false;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return item && cJSON_IsNumber(item);
}

bool JsonHelper::isObject(const cJSON* json, const char* key) {
    if (!json) return false;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return item && cJSON_IsObject(item);
}

bool JsonHelper::isArray(const cJSON* json, const char* key) {
    if (!json) return false;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return item && cJSON_IsArray(item);
}

bool JsonHelper::isBool(const cJSON* json, const char* key) {
    if (!json) return false;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return item && cJSON_IsBool(item);
}

std::string JsonHelper::getString(const cJSON* json, const char* key, const std::string& defaultValue) {
    if (!json) return defaultValue;
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (!item || !cJSON_IsString(item)) return defaultValue;
    return item->valuestring ? std::string(item->valuestring) : defaultValue;
}

int JsonHelper::getInt(const cJSON* json, const char* key, int defaultValue) {
    if (!json) return defaultValue;
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (!item || !cJSON_IsNumber(item)) return defaultValue;
    return static_cast<int>(item->valuedouble);
}

double JsonHelper::getDouble(const cJSON* json, const char* key, double defaultValue) {
    if (!json) return defaultValue;
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (!item || !cJSON_IsNumber(item)) return defaultValue;
    return item->valuedouble;
}

bool JsonHelper::getBool(const cJSON* json, const char* key, bool defaultValue) {
    if (!json) return defaultValue;
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (!item || !cJSON_IsBool(item)) return defaultValue;
    return cJSON_IsTrue(item);
}

cJSON* JsonHelper::getObject(const cJSON* json, const char* key) {
    if (!json) return nullptr;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return (item && cJSON_IsObject(item)) ? item : nullptr;
}

cJSON* JsonHelper::getArray(const cJSON* json, const char* key) {
    if (!json) return nullptr;
    cJSON* item = cJSON_GetObjectItem(json, key);
    return (item && cJSON_IsArray(item)) ? item : nullptr;
}

bool JsonHelper::setString(cJSON* json, const char* key, const std::string& value) {
    if (!json) return false;
    cJSON* item = cJSON_CreateString(value.c_str());
    if (!item) return false;
    cJSON_AddItemToObject(json, key, item);
    return true;
}

bool JsonHelper::setInt(cJSON* json, const char* key, int value) {
    if (!json) return false;
    cJSON* item = cJSON_CreateNumber(value);
    if (!item) return false;
    cJSON_AddItemToObject(json, key, item);
    return true;
}

bool JsonHelper::setDouble(cJSON* json, const char* key, double value) {
    if (!json) return false;
    cJSON* item = cJSON_CreateNumber(value);
    if (!item) return false;
    cJSON_AddItemToObject(json, key, item);
    return true;
}

bool JsonHelper::setBool(cJSON* json, const char* key, bool value) {
    if (!json) return false;
    cJSON* item = cJSON_CreateBool(value);
    if (!item) return false;
    cJSON_AddItemToObject(json, key, item);
    return true;
}

bool JsonHelper::setObject(cJSON* json, const char* key, cJSON* object) {
    if (!json || !object) return false;
    cJSON_AddItemToObject(json, key, object);
    return true;
}

bool JsonHelper::setArray(cJSON* json, const char* key, cJSON* array) {
    if (!json || !array) return false;
    cJSON_AddItemToObject(json, key, array);
    return true;
}

int JsonHelper::getArraySize(const cJSON* array) {
    if (!array || !cJSON_IsArray(array)) return 0;
    return cJSON_GetArraySize(array);
}

cJSON* JsonHelper::getArrayItem(const cJSON* array, int index) {
    if (!array || !cJSON_IsArray(array)) return nullptr;
    return cJSON_GetArrayItem(array, index);
}

bool JsonHelper::addToArray(cJSON* array, cJSON* item) {
    if (!array || !item || !cJSON_IsArray(array)) return false;
    cJSON_AddItemToArray(array, item);
    return true;
}

bool JsonHelper::validateJsonRpc(const cJSON* json) {
    if (!json || !cJSON_IsObject(json)) return false;
    
    // Check for required "jsonrpc" field with value "2.0"
    std::string version = getString(json, MSG_KEY_JSONRPC);
    return version == JSON_RPC_VERSION;
}

bool JsonHelper::validateRequest(const cJSON* json) {
    if (!validateJsonRpc(json)) return false;
    
    // Request must have "method" and "id" fields
    if (!hasField(json, MSG_KEY_METHOD) || !hasField(json, MSG_KEY_ID)) {
        return false;
    }
    
    // Method must be a string
    if (!isString(json, MSG_KEY_METHOD)) return false;
    
    // ID can be string, number, or null (but not missing)
    cJSON* id = cJSON_GetObjectItem(json, MSG_KEY_ID);
    if (!id) return false;
    
    return cJSON_IsString(id) || cJSON_IsNumber(id) || cJSON_IsNull(id);
}

bool JsonHelper::validateResponse(const cJSON* json) {
    if (!validateJsonRpc(json)) return false;
    
    // Response must have "id" field
    if (!hasField(json, MSG_KEY_ID)) return false;
    
    // Must have either "result" or "error" but not both
    bool hasResult = hasField(json, MSG_KEY_RESULT);
    bool hasError = hasField(json, MSG_KEY_ERROR);
    
    return (hasResult && !hasError) || (!hasResult && hasError);
}

bool JsonHelper::validateNotification(const cJSON* json) {
    if (!validateJsonRpc(json)) return false;
    
    // Notification must have "method" field and must NOT have "id" field
    return hasField(json, MSG_KEY_METHOD) && !hasField(json, MSG_KEY_ID);
}

DataType JsonHelper::getIdType(const cJSON* json) {
    if (!json) return DataType::UNKNOWN;
    cJSON* id = cJSON_GetObjectItem(json, MSG_KEY_ID);
    if (!id) return DataType::UNKNOWN;
    
    if (cJSON_IsString(id)) return DataType::STRING;
    if (cJSON_IsNumber(id)) return DataType::INTEGER;
    
    return DataType::UNKNOWN;
}

std::string JsonHelper::getIdAsString(const cJSON* json) {
    if (!json) return "";
    cJSON* id = cJSON_GetObjectItem(json, MSG_KEY_ID);
    if (!id) return "";
    
    if (cJSON_IsString(id)) {
        return id->valuestring ? std::string(id->valuestring) : "";
    }
    
    if (cJSON_IsNumber(id)) {
        return std::to_string(static_cast<int>(id->valuedouble));
    }
    
    return "";
}

int JsonHelper::getIdAsInt(const cJSON* json) {
    if (!json) return 0;
    cJSON* id = cJSON_GetObjectItem(json, MSG_KEY_ID);
    if (!id) return 0;
    
    if (cJSON_IsNumber(id)) {
        return static_cast<int>(id->valuedouble);
    }
    
    if (cJSON_IsString(id)) {
        if (id->valuestring) {
            try {
                return std::stoi(id->valuestring);
            } catch (...) {
                return 0;
            }
        }
    }
    
    return 0;
}

bool JsonHelper::setId(cJSON* json, const std::string& id) {
    return setString(json, MSG_KEY_ID, id);
}

bool JsonHelper::setId(cJSON* json, int id) {
    return setInt(json, MSG_KEY_ID, id);
}

std::string JsonHelper::toString(const cJSON* json, bool formatted) {
    if (!json) return "";
    
    char* str = formatted ? cJSON_Print(json) : cJSON_PrintUnformatted(json);
    if (!str) return "";
    
    std::string result(str);
    free(str);
    return result;
}

size_t JsonHelper::getSerializedSize(const cJSON* json) {
    if (!json) return 0;
    
    char* str = cJSON_PrintUnformatted(json);
    if (!str) return 0;
    
    size_t size = strlen(str);
    free(str);
    return size;
}

JsonValue JsonHelper::createErrorResponse(const std::string& id, int code, const std::string& message, const std::string& data) {
    JsonValue response = JsonValue::createObject();
    if (!response.isValid()) return response;
    
    cJSON* json = response.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setString(json, MSG_KEY_ID, id);
    
    cJSON* error = cJSON_CreateObject();
    if (!error) return JsonValue();
    
    setInt(error, MSG_KEY_CODE, code);
    setString(error, MSG_KEY_MESSAGE, message);
    
    if (!data.empty()) {
        setString(error, MSG_KEY_DATA, data);
    }
    
    setObject(json, MSG_KEY_ERROR, error);
    return response;
}

JsonValue JsonHelper::createErrorResponse(int id, int code, const std::string& message, const std::string& data) {
    JsonValue response = JsonValue::createObject();
    if (!response.isValid()) return response;
    
    cJSON* json = response.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setInt(json, MSG_KEY_ID, id);
    
    cJSON* error = cJSON_CreateObject();
    if (!error) return JsonValue();
    
    setInt(error, MSG_KEY_CODE, code);
    setString(error, MSG_KEY_MESSAGE, message);
    
    if (!data.empty()) {
        setString(error, MSG_KEY_DATA, data);
    }
    
    setObject(json, MSG_KEY_ERROR, error);
    return response;
}

JsonValue JsonHelper::createSuccessResponse(const std::string& id, cJSON* result) {
    JsonValue response = JsonValue::createObject();
    if (!response.isValid()) return response;
    
    cJSON* json = response.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setString(json, MSG_KEY_ID, id);
    
    if (result) {
        setObject(json, MSG_KEY_RESULT, result);
    } else {
        cJSON* nullResult = cJSON_CreateNull();
        setObject(json, MSG_KEY_RESULT, nullResult);
    }
    
    return response;
}

JsonValue JsonHelper::createSuccessResponse(int id, cJSON* result) {
    JsonValue response = JsonValue::createObject();
    if (!response.isValid()) return response;
    
    cJSON* json = response.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setInt(json, MSG_KEY_ID, id);
    
    if (result) {
        setObject(json, MSG_KEY_RESULT, result);
    } else {
        cJSON* nullResult = cJSON_CreateNull();
        setObject(json, MSG_KEY_RESULT, nullResult);
    }
    
    return response;
}

JsonValue JsonHelper::createNotification(const std::string& method, cJSON* params) {
    JsonValue notification = JsonValue::createObject();
    if (!notification.isValid()) return notification;
    
    cJSON* json = notification.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setString(json, MSG_KEY_METHOD, method);
    
    if (params) {
        setObject(json, MSG_KEY_PARAMS, params);
    }
    
    return notification;
}

JsonValue JsonHelper::createRequest(const std::string& method, const std::string& id, cJSON* params) {
    JsonValue request = JsonValue::createObject();
    if (!request.isValid()) return request;
    
    cJSON* json = request.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setString(json, MSG_KEY_METHOD, method);
    setString(json, MSG_KEY_ID, id);
    
    if (params) {
        setObject(json, MSG_KEY_PARAMS, params);
    }
    
    return request;
}

JsonValue JsonHelper::createRequest(const std::string& method, int id, cJSON* params) {
    JsonValue request = JsonValue::createObject();
    if (!request.isValid()) return request;
    
    cJSON* json = request.get();
    setString(json, MSG_KEY_JSONRPC, JSON_RPC_VERSION);
    setString(json, MSG_KEY_METHOD, method);
    setInt(json, MSG_KEY_ID, id);
    
    if (params) {
        setObject(json, MSG_KEY_PARAMS, params);
    }
    
    return request;
}

void JsonHelper::safeDelete(cJSON* json) {
    if (json) {
        cJSON_Delete(json);
    }
}

cJSON* JsonHelper::safeDuplicate(const cJSON* json) {
    return json ? cJSON_Duplicate(json, true) : nullptr;
}

JsonHelper::ValidationResult JsonHelper::validateMessage(const cJSON* json) {
    if (!json) {
        return ValidationResult(false, TINYMCP_PARSE_ERROR, "Invalid JSON");
    }
    
    if (!cJSON_IsObject(json)) {
        return ValidationResult(false, TINYMCP_INVALID_REQUEST, "JSON must be an object");
    }
    
    // Check JSON-RPC version
    if (!validateJsonRpc(json)) {
        return ValidationResult(false, TINYMCP_INVALID_REQUEST, "Invalid JSON-RPC version");
    }
    
    bool hasMethod = hasField(json, MSG_KEY_METHOD);
    bool hasId = hasField(json, MSG_KEY_ID);
    bool hasResult = hasField(json, MSG_KEY_RESULT);
    bool hasError = hasField(json, MSG_KEY_ERROR);
    
    if (hasMethod && hasId) {
        // Request
        if (!isString(json, MSG_KEY_METHOD)) {
            return ValidationResult(false, TINYMCP_INVALID_REQUEST, "Method must be a string");
        }
        return ValidationResult(true);
    } else if (hasMethod && !hasId) {
        // Notification
        if (!isString(json, MSG_KEY_METHOD)) {
            return ValidationResult(false, TINYMCP_INVALID_REQUEST, "Method must be a string");
        }
        return ValidationResult(true);
    } else if (!hasMethod && hasId && (hasResult || hasError)) {
        // Response
        if (hasResult && hasError) {
            return ValidationResult(false, TINYMCP_INVALID_REQUEST, "Response cannot have both result and error");
        }
        return ValidationResult(true);
    }
    
    return ValidationResult(false, TINYMCP_INVALID_REQUEST, "Invalid message structure");
}

JsonHelper::ValidationResult JsonHelper::validateMethodCall(const cJSON* json, const std::string& expectedMethod) {
    ValidationResult result = validateMessage(json);
    if (!result.isValid) return result;
    
    std::string method = getString(json, MSG_KEY_METHOD);
    if (method != expectedMethod) {
        return ValidationResult(false, TINYMCP_METHOD_NOT_FOUND, "Method not found: " + expectedMethod);
    }
    
    return ValidationResult(true);
}

cJSON* JsonHelper::createTextContent(const std::string& text) {
    cJSON* content = cJSON_CreateObject();
    if (!content) return nullptr;
    
    setString(content, MSG_KEY_TYPE, "text");
    setString(content, MSG_KEY_TEXT, text);
    
    return content;
}

cJSON* JsonHelper::createErrorContent(const std::string& error) {
    cJSON* content = cJSON_CreateObject();
    if (!content) return nullptr;
    
    setString(content, MSG_KEY_TYPE, "text");
    setString(content, MSG_KEY_TEXT, error);
    setBool(content, MSG_KEY_IS_ERROR, true);
    
    return content;
}

cJSON* JsonHelper::createBasicToolSchema(const std::string& type) {
    cJSON* schema = cJSON_CreateObject();
    if (!schema) return nullptr;
    
    setString(schema, MSG_KEY_TYPE, type);
    
    return schema;
}

bool JsonHelper::validateAgainstSchema(const cJSON* data, const cJSON* schema) {
    // Basic schema validation - can be extended
    if (!data || !schema) return false;
    
    std::string schemaType = getString(schema, MSG_KEY_TYPE, "object");
    
    if (schemaType == "object") {
        return cJSON_IsObject(data);
    } else if (schemaType == "array") {
        return cJSON_IsArray(data);
    } else if (schemaType == "string") {
        return cJSON_IsString(data);
    } else if (schemaType == "number") {
        return cJSON_IsNumber(data);
    } else if (schemaType == "boolean") {
        return cJSON_IsBool(data);
    }
    
    return true; // Default to valid for unknown types
}

cJSON* JsonHelper::createProgressData(int progress, int total) {
    cJSON* progressData = cJSON_CreateObject();
    if (!progressData) return nullptr;
    
    setInt(progressData, MSG_KEY_PROGRESS, progress);
    setInt(progressData, MSG_KEY_TOTAL, total);
    
    return progressData;
}

JsonValue JsonHelper::createProgressNotification(const std::string& progressToken, int progress, int total) {
    cJSON* params = cJSON_CreateObject();
    if (!params) return JsonValue();
    
    setString(params, MSG_KEY_PROGRESS_TOKEN, progressToken);
    setInt(params, MSG_KEY_PROGRESS, progress);
    setInt(params, MSG_KEY_TOTAL, total);
    
    return createNotification(METHOD_PROGRESS, params);
}

cJSON* JsonHelper::createServerCapabilities(bool toolsListChanged, bool progressNotifications) {
    cJSON* capabilities = cJSON_CreateObject();
    if (!capabilities) return nullptr;
    
    cJSON* tools = cJSON_CreateObject();
    if (tools) {
        setBool(tools, "listChanged", toolsListChanged);
        setObject(capabilities, MSG_KEY_TOOLS, tools);
    }
    
    if (progressNotifications) {
        // Add progress notification capability
        cJSON* logging = cJSON_CreateObject();
        if (logging) {
            setObject(capabilities, "logging", logging);
        }
    }
    
    return capabilities;
}

bool JsonHelper::exceedsMaxSize(const cJSON* json, size_t maxSize) {
    return getSerializedSize(json) > maxSize;
}

size_t JsonHelper::estimateMemoryUsage(const cJSON* json) {
    if (!json) return 0;
    
    return calculateObjectSize(json);
}

// Private helper methods

bool JsonHelper::isValidJsonRpcVersion(const cJSON* json) {
    return getString(json, MSG_KEY_JSONRPC) == JSON_RPC_VERSION;
}

bool JsonHelper::isValidMethod(const cJSON* json) {
    if (!isString(json, MSG_KEY_METHOD)) return false;
    std::string method = getString(json, MSG_KEY_METHOD);
    return !method.empty() && method.length() <= MAX_METHOD_NAME_LENGTH;
}

bool JsonHelper::isValidId(const cJSON* json) {
    cJSON* id = cJSON_GetObjectItem(json, MSG_KEY_ID);
    return id && (cJSON_IsString(id) || cJSON_IsNumber(id));
}

size_t JsonHelper::calculateObjectSize(const cJSON* json) {
    if (!json) return 0;
    
    size_t size = sizeof(cJSON);
    
    if (cJSON_IsString(json)) {
        if (json->valuestring) size += strlen(json->valuestring);
    } else if (cJSON_IsArray(json)) {
        size += calculateArraySize(json);
    } else if (cJSON_IsObject(json)) {
        cJSON* child = json->child;
        while (child) {
            size += calculateObjectSize(child);
            if (child->string) {
                size += strlen(child->string);
            }
            child = child->next;
        }
    }
    
    return size;
}

size_t JsonHelper::calculateArraySize(const cJSON* json) {
    if (!json || !cJSON_IsArray(json)) return 0;
    
    size_t size = sizeof(cJSON);
    cJSON* child = json->child;
    while (child) {
        size += calculateObjectSize(child);
        child = child->next;
    }
    
    return size;
}

size_t JsonHelper::calculateStringSize(const cJSON* json) {
    if (!json || !cJSON_IsString(json)) return 0;
    
    return json->valuestring ? strlen(json->valuestring) : 0;
}

} // namespace tinymcp