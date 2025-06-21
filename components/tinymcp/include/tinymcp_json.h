#pragma once

// JSON Serialization and Parsing Helpers for TinyMCP
// Lightweight JSON utilities for ESP32/ESP8266 with cJSON

#include <string>
#include <memory>
#include "tinymcp_constants.h"

namespace tinymcp {

// Forward declarations
namespace Json {
    class Value;
}

// RAII wrapper for JSON objects
class JsonValue {
public:
    JsonValue();
    explicit JsonValue(const Json::Value& val);
    JsonValue(const JsonValue& other);
    JsonValue(JsonValue&& other) noexcept;
    JsonValue& operator=(const JsonValue& other);
    JsonValue& operator=(JsonValue&& other) noexcept;
    ~JsonValue();
    
    bool isValid() const;
    
    // Static factory methods
    static JsonValue createObject();
    static JsonValue createArray();
    static JsonValue parse(const std::string& jsonStr);

    // Type checking
    bool isObject() const;
    bool isArray() const;
    bool isString() const;
    bool isNumber() const;
    bool isBool() const;
    bool isNull() const;
    
    // Value getters
    std::string asString() const;
    int asInt() const;
    double asDouble() const;
    bool asBool() const;
    
    // Object member access
    JsonValue get(const std::string& key, const JsonValue& defaultValue = JsonValue()) const;
    bool isMember(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, const char* value);
    void set(const std::string& key, int value);
    void set(const std::string& key, double value);
    void set(const std::string& key, bool value);
    void set(const std::string& key, const JsonValue& value);
    
    // Array operations
    JsonValue operator[](int index) const;
    void append(const JsonValue& value);
    void append(const std::string& value);
    int size() const;
    
    // Serialization
    std::string toString() const;
    std::string toStringCompact() const;
    
    // Internal use
    const Json::Value& getInternalValue() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// JSON utility functions
class JsonHelper {
public:
    // Parsing utilities
    static bool hasField(const JsonValue& json, const char* key);
    static bool isString(const JsonValue& json, const char* key);
    static bool isNumber(const JsonValue& json, const char* key);
    static bool isObject(const JsonValue& json, const char* key);
    static bool isArray(const JsonValue& json, const char* key);
    static bool isBool(const JsonValue& json, const char* key);
    
    // Get values with default fallbacks
    static std::string getString(const JsonValue& json, const char* key, const std::string& defaultValue = "");
    static int getInt(const JsonValue& json, const char* key, int defaultValue = 0);
    static double getDouble(const JsonValue& json, const char* key, double defaultValue = 0.0);
    static bool getBool(const JsonValue& json, const char* key, bool defaultValue = false);
    static JsonValue getObject(const JsonValue& json, const char* key);
    static JsonValue getArray(const JsonValue& json, const char* key);
    
    // Set values safely
    static bool setString(JsonValue& json, const char* key, const std::string& value);
    static bool setInt(JsonValue& json, const char* key, int value);
    static bool setDouble(JsonValue& json, const char* key, double value);
    static bool setBool(JsonValue& json, const char* key, bool value);
    static bool setObject(JsonValue& json, const char* key, const JsonValue& object);
    static bool setArray(JsonValue& json, const char* key, const JsonValue& array);
    
    // Array utilities
    static int getArraySize(const JsonValue& array);
    static JsonValue getArrayItem(const JsonValue& array, int index);
    static bool addToArray(JsonValue& array, const JsonValue& item);
    
    // Validation utilities
    static bool validateJsonRpc(const JsonValue& json);
    static bool validateRequest(const JsonValue& json);
    static bool validateResponse(const JsonValue& json);
    static bool validateNotification(const JsonValue& json);
    
    // ID handling (can be string or integer)
    static DataType getIdType(const JsonValue& json);
    static std::string getIdAsString(const JsonValue& json);
    static int getIdAsInt(const JsonValue& json);
    static bool setId(JsonValue& json, const std::string& id);
    static bool setId(JsonValue& json, int id);
    
    // Serialization
    static std::string toString(const JsonValue& json, bool formatted = false);
    static size_t getSerializedSize(const JsonValue& json);
    
    // Error response creation
    static JsonValue createErrorResponse(const std::string& id, int code, const std::string& message, const std::string& data = "");
    static JsonValue createErrorResponse(int id, int code, const std::string& message, const std::string& data = "");
    
    // Success response creation  
    static JsonValue createSuccessResponse(const std::string& id, const JsonValue& result);
    static JsonValue createSuccessResponse(int id, const JsonValue& result);
    
    // Notification creation
    static JsonValue createNotification(const std::string& method, const JsonValue& params = JsonValue());
    
    // Request creation
    static JsonValue createRequest(const std::string& method, const std::string& id, const JsonValue& params = JsonValue());
    static JsonValue createRequest(const std::string& method, int id, const JsonValue& params = JsonValue());
    
    // Validation with detailed error reporting
    struct ValidationResult {
        bool isValid;
        int errorCode;
        std::string errorMessage;
        
        ValidationResult(bool valid = true, int code = 0, const std::string& msg = "") 
            : isValid(valid), errorCode(code), errorMessage(msg) {}
    };
    
    static ValidationResult validateMessage(const JsonValue& json);
    static ValidationResult validateMethodCall(const JsonValue& json, const std::string& expectedMethod);
    
    // Content creation helpers
    static JsonValue createTextContent(const std::string& text);
    static JsonValue createErrorContent(const std::string& error);
    
    // Tool schema helpers
    static JsonValue createBasicToolSchema(const std::string& type = "object");
    static bool validateAgainstSchema(const JsonValue& data, const JsonValue& schema);
    
    // Progress helpers
    static JsonValue createProgressData(int progress, int total = 100);
    static JsonValue createProgressNotification(const std::string& progressToken, int progress, int total = 100);
    
    // Capability helpers
    static JsonValue createServerCapabilities(bool toolsListChanged = true, bool progressNotifications = true);
    
    // Size and memory utilities
    static bool exceedsMaxSize(const JsonValue& json, size_t maxSize = MAX_MESSAGE_SIZE);
    static size_t estimateMemoryUsage(const JsonValue& json);
    
private:
    // Internal validation helpers
    static bool isValidJsonRpcVersion(const JsonValue& json);
    static bool isValidMethod(const JsonValue& json);
    static bool isValidId(const JsonValue& json);
    
    // Memory tracking for ESP32/ESP8266
    static size_t calculateObjectSize(const JsonValue& json);
    static size_t calculateArraySize(const JsonValue& json);
    static size_t calculateStringSize(const JsonValue& json);
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