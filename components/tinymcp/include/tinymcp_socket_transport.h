#pragma once

// ESP Socket Transport Implementation for TinyMCP Session Management
// Provides socket-based communication for ESP8266/ESP32 platforms

#include "tinymcp_session.h"
#include <string>
#include <memory>
#include <atomic>

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

namespace tinymcp {

// Socket transport configuration
struct SocketTransportConfig {
    uint32_t receiveTimeoutMs;      // Socket receive timeout
    uint32_t sendTimeoutMs;         // Socket send timeout
    size_t maxMessageSize;          // Maximum message size
    size_t receiveBufferSize;       // Receive buffer size
    bool enableKeepAlive;           // TCP keep-alive
    uint32_t keepAliveIdleSeconds;  // Keep-alive idle time
    uint32_t keepAliveIntervalSeconds; // Keep-alive interval
    uint32_t keepAliveCount;        // Keep-alive probe count
    
    SocketTransportConfig() :
        receiveTimeoutMs(5000),
        sendTimeoutMs(5000),
        maxMessageSize(8192),
        receiveBufferSize(4096),
        enableKeepAlive(true),
        keepAliveIdleSeconds(60),
        keepAliveIntervalSeconds(10),
        keepAliveCount(3) {}
};

// ESP Socket Transport implementation
class EspSocketTransport : public SessionTransport {
public:
    // Constructor with existing socket
    explicit EspSocketTransport(int socket, const SocketTransportConfig& config = SocketTransportConfig());
    
    // Constructor with address/port (for client connections)
    EspSocketTransport(const std::string& host, uint16_t port, const SocketTransportConfig& config = SocketTransportConfig());
    
    virtual ~EspSocketTransport();
    
    // Delete copy constructor and assignment
    EspSocketTransport(const EspSocketTransport&) = delete;
    EspSocketTransport& operator=(const EspSocketTransport&) = delete;
    
    // SessionTransport interface implementation
    int send(const std::string& data) override;
    int receive(std::string& data, uint32_t timeoutMs = 1000) override;
    bool isConnected() const override;
    void close() override;
    
    std::string getClientInfo() const override;
    size_t getMaxMessageSize() const override { return config_.maxMessageSize; }
    
    // Socket-specific methods
    int connect();
    int setSocketOptions();
    int getSocketError() const;
    
    // Statistics
    struct TransportStats {
        uint64_t bytesSent;
        uint64_t bytesReceived;
        uint32_t messagesSent;
        uint32_t messagesReceived;
        uint32_t sendErrors;
        uint32_t receiveErrors;
        uint32_t timeouts;
        
        TransportStats() : bytesSent(0), bytesReceived(0), messagesSent(0),
                          messagesReceived(0), sendErrors(0), receiveErrors(0), timeouts(0) {}
    };
    
    const TransportStats& getStats() const { return stats_; }
    void resetStats();
    
private:
    // Connection management
    int connectToHost();
    int configureSocket();
    
    // Message framing helpers
    int sendFrame(const std::string& data);
    int receiveFrame(std::string& data, uint32_t timeoutMs);
    int receiveExact(void* buffer, size_t size, uint32_t timeoutMs);
    
    // Utility methods
    std::string formatClientAddress() const;
    bool isSocketValid() const { return socket_ >= 0; }
    
    // Configuration
    SocketTransportConfig config_;
    
    // Socket state
    int socket_;
    std::string hostAddress_;
    uint16_t port_;
    bool isServer_;
    std::atomic<bool> connected_;
    
    // Client information
    struct sockaddr_in clientAddr_;
    std::string clientInfo_;
    
    // Buffers
    std::unique_ptr<char[]> receiveBuffer_;
    
    // Statistics
    mutable TransportStats stats_;
    
    // Constants
    static const uint32_t MESSAGE_HEADER_SIZE = 4; // 4-byte length prefix
    static const uint32_t MAX_RETRIES = 3;
};

// Socket server for accepting connections
class EspSocketServer {
public:
    explicit EspSocketServer(uint16_t port, const SocketTransportConfig& config = SocketTransportConfig());
    ~EspSocketServer();
    
    // Delete copy constructor and assignment
    EspSocketServer(const EspSocketServer&) = delete;
    EspSocketServer& operator=(const EspSocketServer&) = delete;
    
    // Server operations
    int start();
    int stop();
    bool isRunning() const { return running_; }
    
    // Accept incoming connections
    std::unique_ptr<EspSocketTransport> acceptConnection(uint32_t timeoutMs = 0);
    
    // Server information
    uint16_t getPort() const { return port_; }
    size_t getActiveConnections() const { return activeConnections_; }
    
    // Configuration
    void setMaxConnections(size_t maxConnections) { maxConnections_ = maxConnections; }
    void setReuseAddress(bool reuse) { reuseAddress_ = reuse; }
    
private:
    // Socket setup
    int createListenSocket();
    int bindSocket();
    int startListening();
    
    // Configuration
    SocketTransportConfig config_;
    uint16_t port_;
    size_t maxConnections_;
    bool reuseAddress_;
    
    // Server state
    int listenSocket_;
    std::atomic<bool> running_;
    std::atomic<size_t> activeConnections_;
    
    // Server statistics
    struct ServerStats {
        uint32_t connectionsAccepted;
        uint32_t connectionsClosed;
        uint32_t acceptErrors;
        
        ServerStats() : connectionsAccepted(0), connectionsClosed(0), acceptErrors(0) {}
    };
    
    mutable ServerStats stats_;
};

// Utility functions
namespace SocketUtils {
    // Convert error codes to strings
    std::string errorToString(int error);
    
    // Socket option helpers
    int setSocketTimeout(int socket, uint32_t timeoutMs, bool receive = true);
    int setSocketKeepAlive(int socket, bool enable, uint32_t idle = 60, uint32_t interval = 10, uint32_t count = 3);
    int setSocketNonBlocking(int socket, bool nonBlocking = true);
    int setSocketReuseAddress(int socket, bool reuse = true);
    
    // Address helpers
    std::string formatAddress(const struct sockaddr_in& addr);
    int resolveHostname(const std::string& hostname, struct sockaddr_in& addr);
    
    // Network diagnostics
    bool isNetworkAvailable();
    int getSocketError(int socket);
}

// Message framing protocol
class MessageFraming {
public:
    // Frame format: [4-byte length][message data]
    static int encodeMessage(const std::string& message, std::string& frame);
    static int decodeMessage(const std::string& frame, std::string& message);
    
    // Streaming helpers
    static int sendFramedMessage(int socket, const std::string& message, uint32_t timeoutMs);
    static int receiveFramedMessage(int socket, std::string& message, uint32_t timeoutMs);
    
private:
    static const uint32_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB limit
    static const uint32_t HEADER_SIZE = 4;
};

} // namespace tinymcp