#pragma once

#include "cJSON.h"
#include <string>
#include <memory>

namespace tinymcp {

class JsonValue {
private:
    cJSON* m_json;
    bool m_owner;

public:
    JsonValue() : m_json(nullptr), m_owner(false) {}

    JsonValue(cJSON* json, bool owner = false) : m_json(json), m_owner(owner) {}

    ~JsonValue() {
        if (m_owner && m_json) {
            cJSON_Delete(m_json);
        }
    }

    // Copy constructor
    JsonValue(const JsonValue& other) : m_owner(false) {
        if (other.m_json) {
            m_json = cJSON_Duplicate(other.m_json, 1);
            m_owner = true;
        } else {
            m_json = nullptr;
        }
    }

    // Assignment operator
    JsonValue& operator=(const JsonValue& other) {
        if (this != &other) {
            if (m_owner && m_json) {
                cJSON_Delete(m_json);
            }
            if (other.m_json) {
                m_json = cJSON_Duplicate(other.m_json, 1);
                m_owner = true;
            } else {
                m_json = nullptr;
                m_owner = false;
            }
        }
        return *this;
    }

    // Static factory methods
    static JsonValue createObject() {
        return JsonValue(cJSON_CreateObject(), true);
    }

    static JsonValue createArray() {
        return JsonValue(cJSON_CreateArray(), true);
    }

    static JsonValue createString(const std::string& str) {
        return JsonValue(cJSON_CreateString(str.c_str()), true);
    }

    static JsonValue createNumber(double num) {
        return JsonValue(cJSON_CreateNumber(num), true);
    }

    static JsonValue createBool(bool val) {
        return JsonValue(val ? cJSON_CreateTrue() : cJSON_CreateFalse(), true);
    }

    static JsonValue createNull() {
        return JsonValue(cJSON_CreateNull(), true);
    }

    // Type checking
    bool isObject() const { return m_json && cJSON_IsObject(m_json); }
    bool isArray() const { return m_json && cJSON_IsArray(m_json); }
    bool isString() const { return m_json && cJSON_IsString(m_json); }
    bool isNumber() const { return m_json && cJSON_IsNumber(m_json); }
    bool isBool() const { return m_json && cJSON_IsBool(m_json); }
    bool isNull() const { return m_json && cJSON_IsNull(m_json); }
    bool isValid() const { return m_json != nullptr; }

    // Value getters
    std::string asString() const {
        if (isString() && m_json->valuestring) {
            return std::string(m_json->valuestring);
        }
        return "";
    }

    int asInt() const {
        if (isNumber()) {
            return (int)m_json->valuedouble;
        }
        return 0;
    }

    double asDouble() const {
        if (isNumber()) {
            return m_json->valuedouble;
        }
        return 0.0;
    }

    bool asBool() const {
        if (isBool()) {
            return cJSON_IsTrue(m_json);
        }
        return false;
    }

    // Object access
    JsonValue get(const std::string& key, const JsonValue& defaultValue = JsonValue()) const {
        if (isObject()) {
            cJSON* item = cJSON_GetObjectItem(m_json, key.c_str());
            if (item) {
                return JsonValue(item, false);
            }
        }
        return defaultValue;
    }

    bool isMember(const std::string& key) const {
        if (isObject()) {
            return cJSON_GetObjectItem(m_json, key.c_str()) != nullptr;
        }
        return false;
    }

    void set(const std::string& key, const JsonValue& value) {
        if (isObject() && value.m_json) {
            cJSON_AddItemToObject(m_json, key.c_str(), cJSON_Duplicate(value.m_json, 1));
        }
    }

    void set(const std::string& key, const std::string& value) {
        if (isObject()) {
            cJSON_AddStringToObject(m_json, key.c_str(), value.c_str());
        }
    }

    void set(const std::string& key, int value) {
        if (isObject()) {
            cJSON_AddNumberToObject(m_json, key.c_str(), value);
        }
    }

    void set(const std::string& key, double value) {
        if (isObject()) {
            cJSON_AddNumberToObject(m_json, key.c_str(), value);
        }
    }

    void set(const std::string& key, bool value) {
        if (isObject()) {
            cJSON_AddBoolToObject(m_json, key.c_str(), value);
        }
    }

    // Array access
    JsonValue operator[](int index) const {
        if (isArray()) {
            cJSON* item = cJSON_GetArrayItem(m_json, index);
            if (item) {
                return JsonValue(item, false);
            }
        }
        return JsonValue();
    }

    void append(const JsonValue& value) {
        if (isArray() && value.m_json) {
            cJSON_AddItemToArray(m_json, cJSON_Duplicate(value.m_json, 1));
        }
    }

    void append(const std::string& value) {
        if (isArray()) {
            cJSON_AddItemToArray(m_json, cJSON_CreateString(value.c_str()));
        }
    }

    int size() const {
        if (isArray()) {
            return cJSON_GetArraySize(m_json);
        }
        return 0;
    }

    // Serialization
    std::string toString() const {
        if (m_json) {
            char* str = cJSON_Print(m_json);
            if (str) {
                std::string result(str);
                free(str);
                return result;
            }
        }
        return "";
    }

    std::string toStringCompact() const {
        if (m_json) {
            char* str = cJSON_PrintUnformatted(m_json);
            if (str) {
                std::string result(str);
                free(str);
                return result;
            }
        }
        return "";
    }
};

class JsonReader {
public:
    bool parse(const std::string& json, JsonValue& root) {
        cJSON* parsed = cJSON_Parse(json.c_str());
        if (parsed) {
            root = JsonValue(parsed, true);
            return true;
        }
        return false;
    }
};

class JsonStreamWriterBuilder {
private:
    bool m_compact;

public:
    JsonStreamWriterBuilder() : m_compact(false) {}

    void operator[](const std::string& key) {
        if (key == "indentation" || key == "indent") {
            m_compact = true;
        }
    }

    std::string writeString(const JsonValue& value) {
        return m_compact ? value.toStringCompact() : value.toString();
    }
};

// Helper function to write JSON as string
inline std::string writeString(JsonStreamWriterBuilder& builder, const JsonValue& value) {
    return builder.writeString(value);
}

} // namespace tinymcp
