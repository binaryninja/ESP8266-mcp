#pragma once

// Base Message Classes for TinyMCP Protocol
// JSON-RPC 2.0 compliant message architecture for ESP32/ESP8266

#include <string>
#include <memory>
#include <chrono>
#include "tinymcp_constants.h"
#include "tinymcp_json.h"

namespace tinymcp {

// Forward declarations
class Request;
class Response;
class Notification;

// Base Message ID class to handle both string and integer IDs
class MessageId {
public:
    MessageId() : type_(DataType::UNKNOWN), intId_(0) {}
    explicit MessageId(const std::string& id) : type_(DataType::STRING), stringId_(id), intId_(0) {}
    explicit MessageId(int id) : type_(DataType::INTEGER), intId_(id) {}

    // Copy constructor
    MessageId(const MessageId& other) = default;
    MessageId& operator=(const MessageId& other) = default;

    DataType getType() const { return type_; }
    bool isValid() const { return type_ != DataType::UNKNOWN; }
    bool isString() const { return type_ == DataType::STRING; }
    bool isInteger() const { return type_ == DataType::INTEGER; }

    std::string asString() const {
        if (type_ == DataType::STRING) return stringId_;
        if (type_ == DataType::INTEGER) return std::to_string(intId_);
        return "";
    }

    int asInteger() const {
        if (type_ == DataType::INTEGER) return intId_;
        if (type_ == DataType::STRING) {
            // Convert string to integer without exceptions
            // Use atoi which returns 0 on error (safe for ESP8266)
            return atoi(stringId_.c_str());
        }
        return 0;
    }

    bool setFromJson(const cJSON* json);
    bool addToJson(cJSON* json) const;

    // Comparison operators
    bool operator==(const MessageId& other) const;
    bool operator!=(const MessageId& other) const { return !(*this == other); }

private:
    DataType type_;
    std::string stringId_;
    int intId_;
};

// Base Message class
class Message {
public:
    Message(MessageType type, MessageCategory category);
    virtual ~Message() = default;

    // Delete copy constructor and assignment (use move semantics)
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;

    // Move constructor and assignment
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;

    // Getters
    MessageType getType() const { return messageType_; }
    MessageCategory getCategory() const { return messageCategory_; }
    uint64_t getTimestamp() const { return timestamp_; }
    bool hasProgressToken() const { return !progressToken_.empty(); }
    const std::string& getProgressToken() const { return progressToken_; }

    // Setters
    void setProgressToken(const std::string& token) { progressToken_ = token; }

    // Core virtual methods that must be implemented by derived classes
    virtual bool isValid() const = 0;
    virtual int serialize(std::string& jsonOut) const = 0;
    virtual int deserialize(const std::string& jsonIn) = 0;

    // JSON-RPC validation
    virtual bool validateJsonRpc(const cJSON* json) const;

    // Factory method for creating messages from JSON
    static std::unique_ptr<Message> createFromJson(const std::string& jsonStr);
    static MessageType detectMessageType(const cJSON* json);
    static MessageCategory detectMessageCategory(const cJSON* json);

    // Utility methods
    bool exceedsMaxSize() const;
    size_t estimateSize() const;

protected:
    // Internal serialization helpers that derived classes implement
    virtual int doSerialize(cJSON* json) const = 0;
    virtual int doDeserialize(const cJSON* json) = 0;

    // Common validation helpers
    bool validateCommonFields(const cJSON* json) const;
    bool addCommonFields(cJSON* json) const;

    MessageType messageType_;
    MessageCategory messageCategory_;
    uint64_t timestamp_;
    std::string progressToken_;

private:
    // Generate timestamp
    static uint64_t generateTimestamp();
};

// Progress Token class for tracking long-running operations
class ProgressToken {
public:
    ProgressToken() = default;
    explicit ProgressToken(const std::string& token) : token_(token) {}

    bool isValid() const { return !token_.empty(); }
    const std::string& get() const { return token_; }
    void set(const std::string& token) { token_ = token; }

    // Generate a unique progress token
    static ProgressToken generate();

    // Comparison operators
    bool operator==(const ProgressToken& other) const { return token_ == other.token_; }
    bool operator!=(const ProgressToken& other) const { return token_ != other.token_; }

private:
    std::string token_;
};

// Content class for message payloads
class Content {
public:
    Content(ContentType type, const std::string& data) : type_(type), data_(data) {}

    ContentType getType() const { return type_; }
    const std::string& getData() const { return data_; }
    void setData(const std::string& data) { data_ = data; }

    bool isValid() const { return !data_.empty(); }

    // Serialization
    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

    // Factory methods
    static Content createText(const std::string& text);
    static Content createError(const std::string& error);

private:
    ContentType type_;
    std::string data_;
};

// Error class for structured error reporting
class Error {
public:
    Error(int code, const std::string& message, const std::string& data = "")
        : code_(code), message_(message), data_(data) {}

    int getCode() const { return code_; }
    const std::string& getMessage() const { return message_; }
    const std::string& getData() const { return data_; }

    bool hasData() const { return !data_.empty(); }
    bool isValid() const { return code_ != 0 && !message_.empty(); }

    // Serialization
    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

    // Standard error factory methods
    static Error parseError(const std::string& details = "");
    static Error invalidRequest(const std::string& details = "");
    static Error methodNotFound(const std::string& method = "");
    static Error invalidParams(const std::string& details = "");
    static Error internalError(const std::string& details = "");
    static Error notInitialized();
    static Error toolError(const std::string& details = "");

private:
    int code_;
    std::string message_;
    std::string data_;
};

// Server Information class
class ServerInfo {
public:
    ServerInfo(const std::string& name, const std::string& version)
        : name_(name), version_(version) {}

    const std::string& getName() const { return name_; }
    const std::string& getVersion() const { return version_; }

    void setName(const std::string& name) { name_ = name; }
    void setVersion(const std::string& version) { version_ = version; }

    bool isValid() const { return !name_.empty() && !version_.empty(); }

    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

private:
    std::string name_;
    std::string version_;
};

// Client Information class
class ClientInfo {
public:
    ClientInfo(const std::string& name, const std::string& version)
        : name_(name), version_(version) {}

    const std::string& getName() const { return name_; }
    const std::string& getVersion() const { return version_; }

    void setName(const std::string& name) { name_ = name; }
    void setVersion(const std::string& version) { version_ = version; }

    bool isValid() const { return !name_.empty() && !version_.empty(); }

    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

private:
    std::string name_;
    std::string version_;
};

// Server Capabilities class
class ServerCapabilities {
public:
    ServerCapabilities()
        : toolsListChanged_(false)
        , toolsPagination_(false)
        , progressNotifications_(false)
        , resourceSubscription_(false) {}

    // Getters
    bool hasToolsListChanged() const { return toolsListChanged_; }
    bool hasToolsPagination() const { return toolsPagination_; }
    bool hasProgressNotifications() const { return progressNotifications_; }
    bool hasResourceSubscription() const { return resourceSubscription_; }

    // Setters
    void setToolsListChanged(bool enabled) { toolsListChanged_ = enabled; }
    void setToolsPagination(bool enabled) { toolsPagination_ = enabled; }
    void setProgressNotifications(bool enabled) { progressNotifications_ = enabled; }
    void setResourceSubscription(bool enabled) { resourceSubscription_ = enabled; }

    cJSON* toJson() const;
    bool fromJson(const cJSON* json);

private:
    bool toolsListChanged_;
    bool toolsPagination_;
    bool progressNotifications_;
    bool resourceSubscription_;
};

// Message validation result
struct MessageValidationResult {
    bool isValid;
    int errorCode;
    std::string errorMessage;
    MessageType detectedType;
    MessageCategory detectedCategory;

    MessageValidationResult(bool valid = true)
        : isValid(valid), errorCode(0), detectedType(MessageType::UNKNOWN),
          detectedCategory(MessageCategory::UNKNOWN) {}

    MessageValidationResult(int code, const std::string& msg)
        : isValid(false), errorCode(code), errorMessage(msg),
          detectedType(MessageType::UNKNOWN), detectedCategory(MessageCategory::UNKNOWN) {}
};

// Message validator utility
class MessageValidator {
public:
    static MessageValidationResult validate(const std::string& jsonStr);
    static MessageValidationResult validate(const cJSON* json);
    static bool isValidJsonRpc(const cJSON* json);
    static bool isValidMethod(const std::string& method);
    static bool isValidId(const cJSON* idJson);

private:
    static MessageType getMessageTypeFromMethod(const std::string& method);
    static bool validateMessageStructure(const cJSON* json, MessageCategory category);
};

} // namespace tinymcp
