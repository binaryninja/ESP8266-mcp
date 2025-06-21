#pragma once

// Extremely simple JSON wrapper using ESP8266's built-in cJSON
// This provides a minimal C++ interface while being very safe with memory

#include <string>
#include <sstream>
#include <cJSON.h>
#include "esp_log.h"

namespace Json {



// Simple JSON value class that wraps cJSON
class Value {
private:
    cJSON* json_;
    bool owns_;

public:
    // Value types
    enum ValueType { nullValue, objectValue, arrayValue };
    
    // Default constructor - creates null value
    Value() : json_(nullptr), owns_(false) {}
    
    // Constructor for specific types
    explicit Value(ValueType type) : owns_(true) {
        switch (type) {
            case objectValue: 
                json_ = cJSON_CreateObject(); 
                break;
            case arrayValue: 
                json_ = cJSON_CreateArray(); 
                break;
            default: 
                json_ = cJSON_CreateNull(); 
                break;
        }
    }
    
    // String constructor
    explicit Value(const std::string& str) : owns_(true) {
        json_ = cJSON_CreateString(str.c_str());
    }
    
    explicit Value(const char* str) : owns_(true) {
        json_ = cJSON_CreateString(str);
    }
    
    // Number constructors
    explicit Value(int val) : owns_(true) {
        json_ = cJSON_CreateNumber(val);
    }
    
    explicit Value(double val) : owns_(true) {
        json_ = cJSON_CreateNumber(val);
    }
    
    // Bool constructor
    explicit Value(bool val) : owns_(true) {
        json_ = val ? cJSON_CreateTrue() : cJSON_CreateFalse();
    }
    
    // Internal constructor (wraps existing cJSON, doesn't own it)
    Value(cJSON* json, bool owns) : json_(json), owns_(owns) {}
    
    // Copy constructor - always creates a duplicate
    Value(const Value& other) : owns_(true) {
        if (other.json_) {
            json_ = cJSON_Duplicate(other.json_, 1);
        } else {
            json_ = nullptr;
            owns_ = false;
        }
    }
    
    // Assignment operator
    Value& operator=(const Value& other) {
        if (this != &other) {
            // Clean up current value if we own it
            if (owns_ && json_) {
                cJSON_Delete(json_);
            }
            
            // Copy from other
            if (other.json_) {
                json_ = cJSON_Duplicate(other.json_, 1);
                owns_ = true;
            } else {
                json_ = nullptr;
                owns_ = false;
            }
        }
        return *this;
    }
    
    // Move constructor
    Value(Value&& other) noexcept : json_(other.json_), owns_(other.owns_) {
        other.json_ = nullptr;
        other.owns_ = false;
    }
    
    // Move assignment
    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            if (owns_ && json_) {
                cJSON_Delete(json_);
            }
            json_ = other.json_;
            owns_ = other.owns_;
            other.json_ = nullptr;
            other.owns_ = false;
        }
        return *this;
    }
    
    // Destructor
    ~Value() {
        if (owns_ && json_) {
            cJSON_Delete(json_);
        }
    }
    
    // Type checking
    bool isNull() const { return !json_ || cJSON_IsNull(json_); }
    bool isBool() const { return json_ && cJSON_IsBool(json_); }
    bool isInt() const { return json_ && cJSON_IsNumber(json_); }
    bool isDouble() const { return json_ && cJSON_IsNumber(json_); }
    bool isNumeric() const { return json_ && cJSON_IsNumber(json_); }
    bool isString() const { return json_ && cJSON_IsString(json_); }
    bool isArray() const { return json_ && cJSON_IsArray(json_); }
    bool isObject() const { return json_ && cJSON_IsObject(json_); }
    
    // Value extraction
    bool asBool() const {
        if (json_ && cJSON_IsBool(json_)) {
            return cJSON_IsTrue(json_);
        }
        return false;
    }
    
    int asInt() const {
        if (json_ && cJSON_IsNumber(json_)) {
            return (int)json_->valuedouble;
        }
        return 0;
    }
    
    double asDouble() const {
        if (json_ && cJSON_IsNumber(json_)) {
            return json_->valuedouble;
        }
        return 0.0;
    }
    
    std::string asString() const {
        if (json_ && cJSON_IsString(json_) && json_->valuestring) {
            return std::string(json_->valuestring);
        }
        return "";
    }
    
    // Object member access
    Value operator[](const std::string& key) {
        if (!json_ || !cJSON_IsObject(json_)) {
            return Value(); // Return null value
        }
        
        cJSON* item = cJSON_GetObjectItem(json_, key.c_str());
        return Value(item, false); // Don't own the returned item
    }
    
    Value get(const std::string& key, const Value& defaultValue) const {
        if (json_ && cJSON_IsObject(json_)) {
            cJSON* item = cJSON_GetObjectItem(json_, key.c_str());
            if (item) {
                return Value(item, false);
            }
        }
        return defaultValue;
    }
    
    bool isMember(const std::string& key) const {
        if (json_ && cJSON_IsObject(json_)) {
            return cJSON_GetObjectItem(json_, key.c_str()) != nullptr;
        }
        return false;
    }
    
    // Array access
    Value operator[](int index) const {
        if (json_ && cJSON_IsArray(json_)) {
            cJSON* item = cJSON_GetArrayItem(json_, index);
            if (item) {
                return Value(item, false);
            }
        }
        return Value(); // null value
    }
    
    void append(const Value& value) {
        if (!json_) {
            json_ = cJSON_CreateArray();
            owns_ = true;
        }
        if (!cJSON_IsArray(json_)) {
            return; // Can't append to non-array
        }
        
        if (value.json_) {
            cJSON_AddItemToArray(json_, cJSON_Duplicate(value.json_, 1));
        }
    }
    
    size_t size() const {
        if (json_) {
            if (cJSON_IsArray(json_) || cJSON_IsObject(json_)) {
                return cJSON_GetArraySize(json_);
            }
        }
        return 0;
    }
    
    // Assignment operators for different types
    Value& operator=(const std::string& val) {
        if (owns_ && json_) cJSON_Delete(json_);
        json_ = cJSON_CreateString(val.c_str());
        owns_ = true;
        return *this;
    }
    
    Value& operator=(const char* val) {
        if (owns_ && json_) cJSON_Delete(json_);
        json_ = cJSON_CreateString(val);
        owns_ = true;
        return *this;
    }
    
    Value& operator=(int val) {
        if (owns_ && json_) cJSON_Delete(json_);
        json_ = cJSON_CreateNumber(val);
        owns_ = true;
        return *this;
    }
    
    Value& operator=(double val) {
        if (owns_ && json_) cJSON_Delete(json_);
        json_ = cJSON_CreateNumber(val);
        owns_ = true;
        return *this;
    }
    
    Value& operator=(bool val) {
        if (owns_ && json_) cJSON_Delete(json_);
        json_ = val ? cJSON_CreateTrue() : cJSON_CreateFalse();
        owns_ = true;
        return *this;
    }
    
    // Internal access
    cJSON* getCJSON() const { return json_; }
    
    // Safe assignment to object
    void setMember(const std::string& key, const Value& value) {
        if (!json_) {
            json_ = cJSON_CreateObject();
            owns_ = true;
        }
        if (!cJSON_IsObject(json_)) {
            return; // Can't set member on non-object
        }
        
        // Remove existing item if present
        cJSON_DeleteItemFromObject(json_, key.c_str());
        
        // Add new item
        if (value.json_) {
            cJSON_AddItemToObject(json_, key.c_str(), cJSON_Duplicate(value.json_, 1));
        }
    }
};

// Reader class for parsing JSON strings
class CharReaderBuilder {
public:
    class CharReader {
    public:
        bool parse(const char* begin, const char* end, Value* root, std::string* errs) {
            if (!begin || !end || !root) {
                if (errs) *errs = "Invalid parameters";
                return false;
            }
            
            std::string jsonStr(begin, end);
            cJSON* json = cJSON_Parse(jsonStr.c_str());
            if (json) {
                *root = Value(json, true);
                return true;
            } else {
                if (errs) {
                    const char* error = cJSON_GetErrorPtr();
                    *errs = error ? std::string("JSON parse error near: ") + error : "Unknown JSON parse error";
                }
                return false;
            }
        }
    };
    
    CharReader* newCharReader() {
        return new CharReader();
    }
};

// Writer class for generating JSON strings
class StreamWriterBuilder {
private:
    bool compact_;
    
public:
    StreamWriterBuilder() : compact_(false) {}
    
    class StreamWriter {
    private:
        bool compact_;
        
    public:
        StreamWriter(bool compact) : compact_(compact) {}
        
        int write(const Value& root, std::ostream* sout) {
            if (!root.getCJSON() || !sout) {
                if (sout) *sout << "null";
                return 0;
            }
            
            char* str = compact_ ? cJSON_PrintUnformatted(root.getCJSON()) : cJSON_Print(root.getCJSON());
            if (str) {
                *sout << str;
                free(str);
                return 0;
            }
            return -1;
        }
    };
    
    StreamWriter* newStreamWriter() {
        return new StreamWriter(compact_);
    }
    
    void operator[](const std::string& key) {
        if (key == "indentation") {
            compact_ = true;
        }
    }
};

} // namespace Json