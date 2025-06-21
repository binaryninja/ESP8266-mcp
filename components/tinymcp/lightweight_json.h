#pragma once

// Direct jsoncpp usage for TinyMCP
// Simplified JSON utilities for ESP32/ESP8266 with jsoncpp

#include <string>
#include <memory>
#include <sstream>
#include <json/json.h>
#include "esp_log.h"

namespace tinymcp {

// Simple wrapper class that matches the original lightweight_json.h API
class JsonValue {
private:
    Json::Value m_json;

public:
    JsonValue() : m_json(Json::nullValue) {}
    
    explicit JsonValue(const Json::Value& val) : m_json(val) {}
    
    JsonValue(const JsonValue& other) : m_json(other.m_json) {}
    
    JsonValue(JsonValue&& other) noexcept : m_json(std::move(other.m_json)) {}
    
    JsonValue& operator=(const JsonValue& other) {
        if (this != &other) {
            m_json = other.m_json;
        }
        return *this;
    }
    
    JsonValue& operator=(JsonValue&& other) noexcept {
        if (this != &other) {
            m_json = std::move(other.m_json);
        }
        return *this;
    }

    // Factory methods
    static JsonValue createObject() {
        return JsonValue(Json::Value(Json::objectValue));
    }
    
    static JsonValue createArray() {
        return JsonValue(Json::Value(Json::arrayValue));
    }
    
    static JsonValue createString(const std::string& str) {
        return JsonValue(Json::Value(str));
    }
    
    static JsonValue createNumber(double num) {
        return JsonValue(Json::Value(num));
    }
    
    static JsonValue createBool(bool val) {
        return JsonValue(Json::Value(val));
    }
    
    static JsonValue createNull() {
        return JsonValue(Json::Value(Json::nullValue));
    }

    // Type checking
    bool isObject() const { return m_json.isObject(); }
    bool isArray() const { return m_json.isArray(); }
    bool isString() const { return m_json.isString(); }
    bool isNumber() const { return m_json.isNumeric(); }
    bool isBool() const { return m_json.isBool(); }
    bool isNull() const { return m_json.isNull(); }
    bool isValid() const { return !m_json.isNull() || m_json.type() == Json::nullValue; }

    // Value getters
    std::string asString() const {
        return m_json.isString() ? m_json.asString() : "";
    }
    
    int asInt() const {
        return m_json.isInt() ? m_json.asInt() : 0;
    }
    
    double asDouble() const {
        return m_json.isDouble() ? m_json.asDouble() : 0.0;
    }
    
    bool asBool() const {
        return m_json.isBool() ? m_json.asBool() : false;
    }

    // Object member access
    JsonValue get(const std::string& key, const JsonValue& defaultValue = JsonValue()) const {
        if (m_json.isObject() && m_json.isMember(key)) {
            return JsonValue(m_json[key]);
        }
        return defaultValue;
    }
    
    bool isMember(const std::string& key) const {
        return m_json.isObject() && m_json.isMember(key);
    }
    
    void set(const std::string& key, const std::string& value) {
        if (!m_json.isObject()) {
            m_json = Json::Value(Json::objectValue);
        }
        m_json[key] = value;
    }
    
    void set(const std::string& key, const char* value) {
        set(key, std::string(value));
    }
    
    void set(const std::string& key, int value) {
        if (!m_json.isObject()) {
            m_json = Json::Value(Json::objectValue);
        }
        m_json[key] = value;
    }
    
    void set(const std::string& key, double value) {
        if (!m_json.isObject()) {
            m_json = Json::Value(Json::objectValue);
        }
        m_json[key] = value;
    }
    
    void set(const std::string& key, bool value) {
        if (!m_json.isObject()) {
            m_json = Json::Value(Json::objectValue);
        }
        m_json[key] = value;
    }
    
    void set(const std::string& key, const JsonValue& value) {
        if (!m_json.isObject()) {
            m_json = Json::Value(Json::objectValue);
        }
        m_json[key] = value.m_json;
    }

    // Array operations
    JsonValue operator[](int index) const {
        if (m_json.isArray() && index >= 0 && index < static_cast<int>(m_json.size())) {
            return JsonValue(m_json[index]);
        }
        return JsonValue();
    }
    
    void append(const JsonValue& value) {
        if (!m_json.isArray()) {
            m_json = Json::Value(Json::arrayValue);
        }
        m_json.append(value.m_json);
    }
    
    void append(const std::string& value) {
        if (!m_json.isArray()) {
            m_json = Json::Value(Json::arrayValue);
        }
        m_json.append(value);
    }
    
    int size() const {
        if (m_json.isArray() || m_json.isObject()) {
            return m_json.size();
        }
        return 0;
    }

    // Serialization
    std::string toString() const {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        std::ostringstream oss;
        writer->write(m_json, &oss);
        return oss.str();
    }
    
    std::string toStringCompact() const {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        builder["commentStyle"] = "None";
        builder["enableYAMLCompatibility"] = false;
        builder["dropNullPlaceholders"] = false;
        builder["useSpecialFloats"] = false;
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        std::ostringstream oss;
        writer->write(m_json, &oss);
        std::string result = oss.str();
        // Remove trailing newline if present
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        return result;
    }

    // Test function for compatibility
    static bool testCJSONOperations() {
        // Test object creation
        JsonValue obj = createObject();
        obj.set("test", "value");
        obj.set("number", 42);
        obj.set("bool", true);
        
        // Test array creation
        JsonValue arr = createArray();
        arr.append("item1");
        arr.append("item2");
        
        // Test nested structures
        obj.set("array", arr);
        
        // Test serialization
        std::string json = obj.toStringCompact();
        
        // Test parsing
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value root;
        std::string errs;
        
        if (!reader->parse(json.c_str(), json.c_str() + json.length(), &root, &errs)) {
            return false;
        }
        
        return true;
    }

    // For internal use
    const Json::Value& getInternalValue() const {
        return m_json;
    }
    
    Json::Value& getInternalValue() {
        return m_json;
    }
};

class JsonReader {
public:
    bool parse(const std::string& json, JsonValue& root) {
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        Json::Value jsonRoot;
        std::string errs;
        
        bool success = reader->parse(json.c_str(), json.c_str() + json.length(), &jsonRoot, &errs);
        
        if (success) {
            root = JsonValue(jsonRoot);
        }
        
        return success;
    }
};

class JsonStreamWriterBuilder {
private:
    bool m_compact;

public:
    JsonStreamWriterBuilder() : m_compact(false) {}
    
    void operator[](const std::string& key) {
        if (key == "indentation" && m_compact) {
            // Compact mode
        }
    }
    
    std::string writeString(const JsonValue& value) {
        return m_compact ? value.toStringCompact() : value.toString();
    }
};

// Helper function
inline std::string writeString(const JsonStreamWriterBuilder& builder, const JsonValue& value) {
    return value.toStringCompact();
}

} // namespace tinymcp