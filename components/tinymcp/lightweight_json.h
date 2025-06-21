#pragma once

#include "cJSON.h"
#include "esp_log.h"
#include <string>
#include <memory>
#include <cstring>

namespace tinymcp {

static const char* JSON_TAG = "JsonValue";

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

    // Move constructor
    JsonValue(JsonValue&& other) noexcept : m_json(other.m_json), m_owner(other.m_owner) {
        other.m_json = nullptr;
        other.m_owner = false;
    }

    // Move assignment operator
    JsonValue& operator=(JsonValue&& other) noexcept {
        if (this != &other) {
            if (m_owner && m_json) {
                cJSON_Delete(m_json);
            }
            m_json = other.m_json;
            m_owner = other.m_owner;
            other.m_json = nullptr;
            other.m_owner = false;
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
            // Validate inputs
            if (key.empty()) {
                return;
            }
            
            // Remove existing item with same key to prevent duplicates
            cJSON_DeleteItemFromObject(m_json, key.c_str());
            
            cJSON* duplicated = cJSON_Duplicate(value.m_json, 1);
            if (duplicated) {
                cJSON_AddItemToObject(m_json, key.c_str(), duplicated);
            }
        }
    }

    void set(const std::string& key, const std::string& value) {
        if (isObject()) {
            // Validate inputs
            if (key.empty()) {
                ESP_LOGE(JSON_TAG, "Empty key provided to set()");
                return;
            }
            
            // Remove existing item with same key to prevent duplicates
            cJSON_DeleteItemFromObject(m_json, key.c_str());
            
            cJSON* str_item = cJSON_CreateString(value.c_str());
            if (str_item) {
                // Validate the created string item
                if (str_item->valuestring && strcmp(str_item->valuestring, value.c_str()) == 0) {
                    cJSON_AddItemToObject(m_json, key.c_str(), str_item);
                } else {
                    ESP_LOGE(JSON_TAG, "String value mismatch after creation: key=%s", key.c_str());
                    cJSON_Delete(str_item);
                }
            } else {
                ESP_LOGE(JSON_TAG, "cJSON_CreateString returned NULL for key=%s value=%s", key.c_str(), value.c_str());
            }
        }
    }

    void set(const std::string& key, int value) {
        if (isObject() && !key.empty()) {
            // Remove existing item with same key to prevent duplicates
            cJSON_DeleteItemFromObject(m_json, key.c_str());
            cJSON_AddNumberToObject(m_json, key.c_str(), value);
        }
    }

    void set(const std::string& key, double value) {
        if (isObject() && !key.empty()) {
            // Remove existing item with same key to prevent duplicates
            cJSON_DeleteItemFromObject(m_json, key.c_str());
            cJSON_AddNumberToObject(m_json, key.c_str(), value);
        }
    }

    void set(const std::string& key, bool value) {
        if (isObject() && !key.empty()) {
            // Remove existing item with same key to prevent duplicates
            cJSON_DeleteItemFromObject(m_json, key.c_str());
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
        if (m_json && !cJSON_IsInvalid(m_json)) {
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
        if (!m_json) {
            ESP_LOGE(JSON_TAG, "m_json is NULL in toStringCompact()");
            return "";
        }

        // Use validation helper first
        if (!isValidStructure()) {
            ESP_LOGE(JSON_TAG, "JSON structure validation failed");
            return "";
        }

        char* str = cJSON_PrintUnformatted(m_json);
        if (str) {
            std::string result(str);
            free(str);
            return result;
        } else {
            ESP_LOGE(JSON_TAG, "cJSON_PrintUnformatted returned NULL even after validation");
            // Try formatted print as fallback
            str = cJSON_Print(m_json);
            if (str) {
                ESP_LOGW(JSON_TAG, "Fallback to formatted print worked");
                std::string result(str);
                free(str);
                return result;
            } else {
                ESP_LOGE(JSON_TAG, "Both formatted and unformatted print failed");
            }
        }
        return "";
    }

    // Validation helper
    bool isValidStructure() const {
        if (!m_json) return false;
        if (cJSON_IsInvalid(m_json)) return false;
        
        // Additional validation for objects and arrays
        if (cJSON_IsObject(m_json)) {
            cJSON* item = m_json->child;
            while (item) {
                if (!item->string) {
                    ESP_LOGE(JSON_TAG, "Found object item with NULL key");
                    return false;
                }
                if (cJSON_IsString(item) && !item->valuestring) {
                    ESP_LOGE(JSON_TAG, "Found string item with NULL valuestring for key: %s", item->string ? item->string : "unknown");
                    return false;
                }
                item = item->next;
            }
        }
        return true;
    }

    // Debug test method to validate cJSON operations
    static bool testCJSONOperations() {
        ESP_LOGI(JSON_TAG, "Testing cJSON operations...");
        
        // Test 1: Simple string creation
        cJSON* test_str = cJSON_CreateString("test_value");
        if (!test_str) {
            ESP_LOGE(JSON_TAG, "cJSON_CreateString failed");
            return false;
        }
        if (!test_str->valuestring || strcmp(test_str->valuestring, "test_value") != 0) {
            ESP_LOGE(JSON_TAG, "String value corrupted after creation");
            cJSON_Delete(test_str);
            return false;
        }
        ESP_LOGI(JSON_TAG, "String creation test passed");
        cJSON_Delete(test_str);
        
        // Test 2: Object with string
        cJSON* test_obj = cJSON_CreateObject();
        if (!test_obj) {
            ESP_LOGE(JSON_TAG, "cJSON_CreateObject failed");
            return false;
        }
        
        cJSON* str_item = cJSON_CreateString("object_value");
        if (!str_item) {
            ESP_LOGE(JSON_TAG, "cJSON_CreateString failed for object");
            cJSON_Delete(test_obj);
            return false;
        }
        
        cJSON_AddItemToObject(test_obj, "test_key", str_item);
        
        // Test serialization
        char* json_str = cJSON_PrintUnformatted(test_obj);
        if (!json_str) {
            ESP_LOGE(JSON_TAG, "cJSON_PrintUnformatted failed on test object");
            cJSON_Delete(test_obj);
            return false;
        }
        
        ESP_LOGI(JSON_TAG, "Test object serialized as: %s", json_str);
        free(json_str);
        cJSON_Delete(test_obj);
        
        ESP_LOGI(JSON_TAG, "All cJSON tests passed");
        return true;
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
