#pragma once

// JSON Serialization and Parsing Helpers for TinyMCP
// Lightweight JSON utilities for ESP32/ESP8266 with cJSON

#include <string>
#include <memory>
#include <cJSON.h>
#include "tinymcp_constants.h"

namespace tinymcp {

// RAII wrapper for cJSON objects
class JsonValue {
public:
    explicit JsonValue(cJSON* json = nullptr) : json_(json) {}
    
    // Move constructor
    JsonValue(JsonValue&& other) noexcept : json_(other.json_) {
        other.json_ = nullptr;
    }
    
    // Move assignment
    JsonValue& operator=(JsonValue&& other) noexcept {
        if (this != &other) {
            if (json_) {
                cJSON_Delete(json_);
            }
            json_ = other.json_;
            other.json_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy constructor and assignment
    JsonValue(const JsonValue&) = delete;
    JsonValue& operator=(const JsonValue&) = delete;
    
    ~JsonValue() {
        if (json_) {
            cJSON_Delete(json_);
        }
    }
    
    cJSON* get() const { return json_; }
    cJSON* release() {
        cJSON* temp = json_;
        json_ = nullptr;
        return temp;
    }
    
    bool isValid() const { return json_ != nullptr; }
    
    // Static factory methods
    static JsonValue createObject() {
        return JsonValue(cJSON_CreateObject());
    }
    
    static JsonValue createArray() {
        return JsonValue(cJSON_CreateArray());
    }
    
    static JsonValue parse(const std::string& jsonStr) {
        return JsonValue(cJSON_Parse(jsonStr.c_str()));
    }

private:
    cJSON* json_;
};

// JSON utility functions
class JsonHelper {
public:
    // Parsing utilities
    static bool hasField(const cJSON* json, const char* key);
    static bool isString(const cJSON* json, const char* key);
    static bool isNumber(const cJSON* json, const char* key);
    static bool isObject(const cJSON* json, const char* key);
    static bool isArray(const cJSON* json, const char* key);
    static bool isBool(const cJSON* json, const char* key);
    
    // Get values with default fallbacks
    static std::string getString(const cJSON* json, const char* key, const std::string& defaultValue = "");
    static int getInt(const cJSON* json, const char* key, int defaultValue = 0);
    static double getDouble(const cJSON* json, const char* key, double defaultValue = 0.0);
    static bool getBool(const cJSON* json, const char* key, bool defaultValue = false);
    static cJSON* getObject(const cJSON* json, const char* key);
    static cJSON* getArray(const cJSON* json, const char* key);
    
    // Set values safely
    static bool setString(cJSON* json, const char* key, const std::string& value);
    static bool setInt(cJSON* json, const char* key, int value);
    static bool setDouble(cJSON* json, const char* key, double value);
    static bool setBool(cJSON* json, const char* key, bool value);
    static bool setObject(cJSON* json, const char* key, cJSON* object);
    static bool setArray(cJSON* json, const char* key, cJSON* array);
    
    // Array utilities
    static int getArraySize(const cJSON* array);
    static cJSON* getArrayItem(const cJSON* array, int index);
    static bool addToArray(cJSON* array, cJSON* item);
    
    // Validation utilities
    static bool validateJsonRpc(const cJSON* json);
    static bool validateRequest(const cJSON* json);
    static bool validateResponse(const cJSON* json);
    static bool validateNotification(const cJSON* json);
    
    // ID handling (can be string or integer)
    static DataType getIdType(const cJSON* json);
    static std::string getIdAsString(const cJSON* json);
    static int getIdAsInt(const cJSON* json);
    static bool setId(cJSON* json, const std::string& id);
    static bool setId(cJSON* json, int id);
    
    // Serialization
    static std::string toString(const cJSON* json, bool formatted = false);
    static size_t getSerializedSize(const cJSON* json);
    
    // Error response creation
    static JsonValue createErrorResponse(const std::string& id, int code, const std::string& message, const std::string& data = "");
    static JsonValue createErrorResponse(int id, int code, const std::string& message, const std::string& data = "");
    
    // Success response creation  
    static JsonValue createSuccessResponse(const std::string& id, cJSON* result);
    static JsonValue createSuccessResponse(int id, cJSON* result);
    
    // Notification creation
    static JsonValue createNotification(const std::string& method, cJSON* params = nullptr);
    
    // Request creation
    static JsonValue createRequest(const std::string& method, const std::string& id, cJSON* params = nullptr);
    static JsonValue createRequest(const std::string& method, int id, cJSON* params = nullptr);
    
    // Memory management helpers
    static void safeDelete(cJSON* json);
    static cJSON* safeDuplicate(const cJSON* json);
    
    // Validation with detailed error reporting
    struct ValidationResult {
        bool isValid;
        int errorCode;
        std::string errorMessage;
        
        ValidationResult(bool valid = true, int code = 0, const std::string& msg = "") 
            : isValid(valid), errorCode(code), errorMessage(msg) {}
    };
    
    static ValidationResult validateMessage(const cJSON* json);
    static ValidationResult validateMethodCall(const cJSON* json, const std::string& expectedMethod);
    
    // Content creation helpers
    static cJSON* createTextContent(const std::string& text);
    static cJSON* createErrorContent(const std::string& error);
    
    // Tool schema helpers
    static cJSON* createBasicToolSchema(const std::string& type = "object");
    static bool validateAgainstSchema(const cJSON* data, const cJSON* schema);
    
    // Progress helpers
    static cJSON* createProgressData(int progress, int total = 100);
    static JsonValue createProgressNotification(const std::string& progressToken, int progress, int total = 100);
    
    // Capability helpers
    static cJSON* createServerCapabilities(bool toolsListChanged = true, bool progressNotifications = true);
    
    // Size and memory utilities
    static bool exceedsMaxSize(const cJSON* json, size_t maxSize = MAX_MESSAGE_SIZE);
    static size_t estimateMemoryUsage(const cJSON* json);
    
private:
    // Internal validation helpers
    static bool isValidJsonRpcVersion(const cJSON* json);
    static bool isValidMethod(const cJSON* json);
    static bool isValidId(const cJSON* json);
    
    // Memory tracking for ESP32/ESP8266
    static size_t calculateObjectSize(const cJSON* json);
    static size_t calculateArraySize(const cJSON* json);
    static size_t calculateStringSize(const cJSON* json);
};

// Convenience macros for common JSON operations
#define JSON_GET_STRING(json, key, defaultVal) \
    JsonHelper::getString(json, key, defaultVal)

#define JSON_GET_INT(json, key, defaultVal) \
    JsonHelper::getInt(json, key, defaultVal)

#define JSON_HAS_FIELD(json, key) \
    JsonHelper::hasField(json, key)

#define JSON_IS_VALID_REQUEST(json) \
    (JsonHelper::validateJsonRpc(json) && JsonHelper::validateRequest(json))

#define JSON_IS_VALID_RESPONSE(json) \
    (JsonHelper::validateJsonRpc(json) && JsonHelper::validateResponse(json))

#define JSON_IS_VALID_NOTIFICATION(json) \
    (JsonHelper::validateJsonRpc(json) && JsonHelper::validateNotification(json))

} // namespace tinymcp