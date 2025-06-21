#pragma once

// Notification Message Classes for TinyMCP Protocol
// JSON-RPC 2.0 compliant notification messages for ESP32/ESP8266

#include <string>
#include <memory>
#include <vector>
#include "tinymcp_message.h"
#include "tinymcp_constants.h"
#include "tinymcp_json.h"

namespace tinymcp {

// Base Notification class
class Notification : public Message {
public:
    Notification(MessageType type, const std::string& method);
    virtual ~Notification() = default;
    
    // Move constructor and assignment
    Notification(Notification&&) = default;
    Notification& operator=(Notification&&) = default;
    
    // Getters
    const std::string& getMethod() const { return method_; }
    
    // Setters
    void setMethod(const std::string& method) { method_ = method; }
    
    // Base Message interface implementation
    bool isValid() const override;
    int serialize(std::string& jsonOut) const override;
    int deserialize(const std::string& jsonIn) override;
    
    // Notification-specific validation
    virtual bool validateParams(const cJSON* params) const { return true; }
    
protected:
    int doSerialize(cJSON* json) const override;
    int doDeserialize(const cJSON* json) override;
    
    // Virtual methods for parameter handling
    virtual int serializeParams(cJSON* json) const { return 0; }
    virtual int deserializeParams(const cJSON* json) { return 0; }
    
    std::string method_;
};

// Initialized Notification
// Sent by client after successful initialization
class InitializedNotification : public Notification {
public:
    InitializedNotification()
        : Notification(MessageType::INITIALIZED_NOTIFICATION, METHOD_INITIALIZED) {}
    
    // Optional meta information
    const std::string& getClientVersion() const { return clientVersion_; }
    void setClientVersion(const std::string& version) { clientVersion_ = version; }
    
    const std::string& getSessionId() const { return sessionId_; }
    void setSessionId(const std::string& sessionId) { sessionId_ = sessionId; }
    
    // Capabilities the client wants to advertise
    bool hasClientCapabilities() const { return clientCapabilities_ != nullptr; }
    const cJSON* getClientCapabilities() const { return clientCapabilities_; }
    void setClientCapabilities(cJSON* capabilities);
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    std::string clientVersion_;
    std::string sessionId_;
    cJSON* clientCapabilities_;

public:
    ~InitializedNotification() {
        if (clientCapabilities_) {
            cJSON_Delete(clientCapabilities_);
        }
    }
};

// Progress Notification
// Sent during long-running operations to report progress
class ProgressNotification : public Notification {
public:
    ProgressNotification(const ProgressToken& token, int progress, int total = 100)
        : Notification(MessageType::PROGRESS_NOTIFICATION, METHOD_PROGRESS)
        , progressToken_(token)
        , progress_(progress)
        , total_(total) {}
    
    // Progress information
    const ProgressToken& getProgressToken() const { return progressToken_; }
    void setProgressToken(const ProgressToken& token) { progressToken_ = token; }
    
    int getProgress() const { return progress_; }
    void setProgress(int progress) { progress_ = progress; }
    
    int getTotal() const { return total_; }
    void setTotal(int total) { total_ = total; }
    
    // Progress percentage (0-100)
    double getProgressPercentage() const {
        if (total_ <= 0) return 0.0;
        return (static_cast<double>(progress_) / total_) * 100.0;
    }
    
    // Optional progress message
    const std::string& getMessage() const { return message_; }
    void setMessage(const std::string& message) { message_ = message; }
    bool hasMessage() const { return !message_.empty(); }
    
    // Optional progress details
    const std::string& getDetails() const { return details_; }
    void setDetails(const std::string& details) { details_ = details; }
    bool hasDetails() const { return !details_.empty(); }
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    ProgressToken progressToken_;
    int progress_;
    int total_;
    std::string message_;
    std::string details_;
};

// Cancelled Notification
// Sent when an operation is cancelled
class CancelledNotification : public Notification {
public:
    CancelledNotification(const std::string& requestId, const std::string& reason = "")
        : Notification(MessageType::CANCELLED_NOTIFICATION, METHOD_CANCELLED)
        , requestId_(requestId)
        , reason_(reason) {}
    
    // Request identification
    const std::string& getRequestId() const { return requestId_; }
    void setRequestId(const std::string& requestId) { requestId_ = requestId; }
    
    // Cancellation reason
    const std::string& getReason() const { return reason_; }
    void setReason(const std::string& reason) { reason_ = reason; }
    bool hasReason() const { return !reason_.empty(); }
    
    // Optional progress token if applicable
    const ProgressToken& getProgressToken() const { return progressToken_; }
    void setProgressToken(const ProgressToken& token) { progressToken_ = token; }
    bool hasProgressToken() const { return progressToken_.isValid(); }
    
    // Error information if cancellation was due to error
    bool isErrorCancellation() const { return errorCode_ != 0; }
    int getErrorCode() const { return errorCode_; }
    const std::string& getErrorMessage() const { return errorMessage_; }
    void setError(int code, const std::string& message) {
        errorCode_ = code;
        errorMessage_ = message;
    }
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    std::string requestId_;
    std::string reason_;
    ProgressToken progressToken_;
    int errorCode_ = 0;
    std::string errorMessage_;
};

// Tools List Changed Notification
// Sent when the available tools change
class ToolsListChangedNotification : public Notification {
public:
    ToolsListChangedNotification()
        : Notification(MessageType::UNKNOWN, "notifications/tools/list_changed") {}
    
    // Optional list of added tools
    const std::vector<std::string>& getAddedTools() const { return addedTools_; }
    void setAddedTools(const std::vector<std::string>& tools) { addedTools_ = tools; }
    void addTool(const std::string& toolName) { addedTools_.push_back(toolName); }
    
    // Optional list of removed tools
    const std::vector<std::string>& getRemovedTools() const { return removedTools_; }
    void setRemovedTools(const std::vector<std::string>& tools) { removedTools_ = tools; }
    void removeTool(const std::string& toolName) { removedTools_.push_back(toolName); }
    
    // Optional list of modified tools
    const std::vector<std::string>& getModifiedTools() const { return modifiedTools_; }
    void setModifiedTools(const std::vector<std::string>& tools) { modifiedTools_ = tools; }
    void modifyTool(const std::string& toolName) { modifiedTools_.push_back(toolName); }
    
    bool hasChanges() const {
        return !addedTools_.empty() || !removedTools_.empty() || !modifiedTools_.empty();
    }
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    std::vector<std::string> addedTools_;
    std::vector<std::string> removedTools_;
    std::vector<std::string> modifiedTools_;
};

// Log Notification (for debugging/monitoring)
class LogNotification : public Notification {
public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };
    
    LogNotification(LogLevel level, const std::string& message)
        : Notification(MessageType::UNKNOWN, "notifications/log")
        , level_(level)
        , message_(message) {}
    
    LogLevel getLevel() const { return level_; }
    void setLevel(LogLevel level) { level_ = level; }
    
    const std::string& getMessage() const { return message_; }
    void setMessage(const std::string& message) { message_ = message; }
    
    // Optional context information
    const std::string& getContext() const { return context_; }
    void setContext(const std::string& context) { context_ = context; }
    bool hasContext() const { return !context_.empty(); }
    
    // Optional data payload
    const cJSON* getData() const { return data_; }
    void setData(cJSON* data);
    bool hasData() const { return data_ != nullptr; }
    
    std::string getLevelString() const {
        switch (level_) {
            case DEBUG: return "debug";
            case INFO: return "info";
            case WARN: return "warn";
            case ERROR: return "error";
            default: return "unknown";
        }
    }
    
    bool validateParams(const cJSON* params) const override;
    
protected:
    int serializeParams(cJSON* json) const override;
    int deserializeParams(const cJSON* json) override;

private:
    LogLevel level_;
    std::string message_;
    std::string context_;
    cJSON* data_;

public:
    ~LogNotification() {
        if (data_) {
            cJSON_Delete(data_);
        }
    }
};

// Notification Factory for creating notifications from JSON
class NotificationFactory {
public:
    static std::unique_ptr<Notification> createFromJson(const std::string& jsonStr);
    static std::unique_ptr<Notification> createFromJson(const cJSON* json);
    
    // Factory methods for specific notification types
    static std::unique_ptr<InitializedNotification> createInitializedNotification(
        const std::string& clientVersion = "",
        const std::string& sessionId = "");
    
    static std::unique_ptr<ProgressNotification> createProgressNotification(
        const ProgressToken& token,
        int progress,
        int total = 100,
        const std::string& message = "");
    
    static std::unique_ptr<CancelledNotification> createCancelledNotification(
        const std::string& requestId,
        const std::string& reason = "");
    
    static std::unique_ptr<ToolsListChangedNotification> createToolsListChangedNotification();
    
    static std::unique_ptr<LogNotification> createLogNotification(
        LogNotification::LogLevel level,
        const std::string& message,
        const std::string& context = "");

private:
    static MessageType getNotificationType(const std::string& method);
};

// Notification validation utilities
class NotificationValidator {
public:
    static bool validateInitializedNotification(const cJSON* json);
    static bool validateProgressNotification(const cJSON* json);
    static bool validateCancelledNotification(const cJSON* json);
    static bool validateToolsListChangedNotification(const cJSON* json);
    static bool validateLogNotification(const cJSON* json);
    
    // Parameter validation helpers
    static bool validateProgressToken(const std::string& token);
    static bool validateRequestId(const std::string& requestId);
    static bool validateLogLevel(const std::string& level);
    static bool validateToolNameList(const cJSON* toolList);

private:
    static bool isValidProgressValue(int progress, int total);
    static bool isValidLogLevel(int level);
};

// Notification builder pattern for easier construction
class NotificationBuilder {
public:
    NotificationBuilder() = default;
    
    // Initialized notification builder
    NotificationBuilder& initializedNotification();
    NotificationBuilder& withClientVersion(const std::string& version);
    NotificationBuilder& withSessionId(const std::string& sessionId);
    NotificationBuilder& withClientCapabilities(cJSON* capabilities);
    
    // Progress notification builder
    NotificationBuilder& progressNotification(const ProgressToken& token);
    NotificationBuilder& withProgress(int progress, int total = 100);
    NotificationBuilder& withProgressMessage(const std::string& message);
    NotificationBuilder& withProgressDetails(const std::string& details);
    
    // Cancelled notification builder
    NotificationBuilder& cancelledNotification(const std::string& requestId);
    NotificationBuilder& withCancellationReason(const std::string& reason);
    NotificationBuilder& withProgressToken(const ProgressToken& token);
    NotificationBuilder& withCancellationError(int code, const std::string& message);
    
    // Tools list changed notification builder
    NotificationBuilder& toolsListChangedNotification();
    NotificationBuilder& withAddedTools(const std::vector<std::string>& tools);
    NotificationBuilder& withRemovedTools(const std::vector<std::string>& tools);
    NotificationBuilder& withModifiedTools(const std::vector<std::string>& tools);
    
    // Log notification builder
    NotificationBuilder& logNotification(LogNotification::LogLevel level, const std::string& message);
    NotificationBuilder& withLogContext(const std::string& context);
    NotificationBuilder& withLogData(cJSON* data);
    
    // Build the final notification
    std::unique_ptr<Notification> build();
    
    // Reset for reuse
    void reset();

private:
    MessageType notificationType_ = MessageType::UNKNOWN;
    std::string method_;
    
    // Initialized notification data
    std::string clientVersion_;
    std::string sessionId_;
    cJSON* clientCapabilities_ = nullptr;
    
    // Progress notification data
    ProgressToken progressToken_;
    int progress_ = 0;
    int total_ = 100;
    std::string progressMessage_;
    std::string progressDetails_;
    
    // Cancelled notification data
    std::string requestId_;
    std::string cancellationReason_;
    int errorCode_ = 0;
    std::string errorMessage_;
    
    // Tools list changed notification data
    std::vector<std::string> addedTools_;
    std::vector<std::string> removedTools_;
    std::vector<std::string> modifiedTools_;
    
    // Log notification data
    LogNotification::LogLevel logLevel_ = LogNotification::INFO;
    std::string logMessage_;
    std::string logContext_;
    cJSON* logData_ = nullptr;
};

} // namespace tinymcp