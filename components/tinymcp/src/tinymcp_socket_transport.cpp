// ESP Socket Transport Implementation for TinyMCP Session Management
// Provides socket-based communication for ESP8266/ESP32 platforms

#include "tinymcp_socket_transport.h"
#include "tinymcp_constants.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <algorithm>

static const char* TAG = "tinymcp_socket";

namespace tinymcp {

// EspSocketTransport implementation
EspSocketTransport::EspSocketTransport(int socket, const SocketTransportConfig& config) :
    config_(config), socket_(socket), port_(0), isServer_(true), connected_(true) {
    
    receiveBuffer_ = std::make_unique<char[]>(config_.receiveBufferSize);
    
    // Get client address information
    socklen_t addrLen = sizeof(clientAddr_);
    if (getpeername(socket_, (struct sockaddr*)&clientAddr_, &addrLen) == 0) {
        clientInfo_ = formatClientAddress();
    } else {
        clientInfo_ = "Unknown client";
    }
    
    // Configure socket options
    configureSocket();
    
    ESP_LOGI(TAG, "Socket transport created for client: %s", clientInfo_.c_str());
}

EspSocketTransport::EspSocketTransport(const std::string& host, uint16_t port, const SocketTransportConfig& config) :
    config_(config), socket_(-1), hostAddress_(host), port_(port), isServer_(false), connected_(false) {
    
    receiveBuffer_ = std::make_unique<char[]>(config_.receiveBufferSize);
    clientInfo_ = host + ":" + std::to_string(port);
    
    ESP_LOGI(TAG, "Socket transport created for server: %s", clientInfo_.c_str());
}

EspSocketTransport::~EspSocketTransport() {
    close();
    ESP_LOGI(TAG, "Socket transport destroyed");
}

int EspSocketTransport::send(const std::string& data) {
    if (!isConnected()) {
        ESP_LOGW(TAG, "Attempt to send on disconnected socket");
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    if (data.empty()) {
        return TINYMCP_SUCCESS;
    }
    
    if (data.size() > config_.maxMessageSize) {
        ESP_LOGW(TAG, "Message too large: %zu bytes", data.size());
        return TINYMCP_ERROR_MESSAGE_TOO_LARGE;
    }
    
    int result = sendFrame(data);
    if (result == TINYMCP_SUCCESS) {
        stats_.bytesSent += data.size();
        stats_.messagesSent++;
    } else {
        stats_.sendErrors++;
    }
    
    return result;
}

int EspSocketTransport::receive(std::string& data, uint32_t timeoutMs) {
    if (!isConnected()) {
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    data.clear();
    
    int result = receiveFrame(data, timeoutMs);
    if (result == TINYMCP_SUCCESS) {
        stats_.bytesReceived += data.size();
        stats_.messagesReceived++;
    } else if (result == TINYMCP_ERROR_TIMEOUT) {
        stats_.timeouts++;
    } else {
        stats_.receiveErrors++;
    }
    
    return result;
}

bool EspSocketTransport::isConnected() const {
    if (!connected_ || !isSocketValid()) {
        return false;
    }
    
    // Check if socket is still valid by trying to get socket error
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
        connected_ = false;
        return false;
    }
    
    return true;
}

void EspSocketTransport::close() {
    if (isSocketValid()) {
        ESP_LOGI(TAG, "Closing socket connection");
        ::close(socket_);
        socket_ = -1;
    }
    connected_ = false;
}

std::string EspSocketTransport::getClientInfo() const {
    return clientInfo_;
}

int EspSocketTransport::connect() {
    if (isServer_) {
        ESP_LOGW(TAG, "Connect called on server socket");
        return TINYMCP_ERROR_INVALID_OPERATION;
    }
    
    if (connected_) {
        return TINYMCP_SUCCESS;
    }
    
    return connectToHost();
}

int EspSocketTransport::connectToHost() {
    // Create socket
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %s", strerror(errno));
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    // Configure socket
    int result = configureSocket();
    if (result != TINYMCP_SUCCESS) {
        ::close(socket_);
        socket_ = -1;
        return result;
    }
    
    // Resolve hostname and connect
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, hostAddress_.c_str(), &serverAddr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "Invalid address: %s", hostAddress_.c_str());
        ::close(socket_);
        socket_ = -1;
        return TINYMCP_ERROR_INVALID_PARAMS;
    }
    
    // Set socket to non-blocking for connect timeout
    int flags = fcntl(socket_, F_GETFL, 0);
    fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
    
    int connectResult = ::connect(socket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (connectResult < 0 && errno != EINPROGRESS) {
        ESP_LOGE(TAG, "Connect failed: %s", strerror(errno));
        ::close(socket_);
        socket_ = -1;
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    // Wait for connection with timeout
    if (connectResult < 0) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(socket_, &writeSet);
        
        struct timeval timeout;
        timeout.tv_sec = config_.sendTimeoutMs / 1000;
        timeout.tv_usec = (config_.sendTimeoutMs % 1000) * 1000;
        
        int selectResult = select(socket_ + 1, NULL, &writeSet, NULL, &timeout);
        if (selectResult <= 0) {
            ESP_LOGE(TAG, "Connect timeout or error");
            ::close(socket_);
            socket_ = -1;
            return TINYMCP_ERROR_TIMEOUT;
        }
        
        // Check if connection succeeded
        int error;
        socklen_t len = sizeof(error);
        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
            ESP_LOGE(TAG, "Connect failed: %s", strerror(error));
            ::close(socket_);
            socket_ = -1;
            return TINYMCP_ERROR_TRANSPORT_FAILED;
        }
    }
    
    // Restore blocking mode
    fcntl(socket_, F_SETFL, flags);
    
    connected_ = true;
    ESP_LOGI(TAG, "Connected to server: %s:%d", hostAddress_.c_str(), port_);
    
    return TINYMCP_SUCCESS;
}

int EspSocketTransport::configureSocket() {
    if (!isSocketValid()) {
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    // Set socket timeouts
    SocketUtils::setSocketTimeout(socket_, config_.receiveTimeoutMs, true);
    SocketUtils::setSocketTimeout(socket_, config_.sendTimeoutMs, false);
    
    // Set keep-alive options
    if (config_.enableKeepAlive) {
        SocketUtils::setSocketKeepAlive(socket_, true, 
                                       config_.keepAliveIdleSeconds,
                                       config_.keepAliveIntervalSeconds,
                                       config_.keepAliveCount);
    }
    
    // Disable Nagle's algorithm for low latency
    int flag = 1;
    setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    return TINYMCP_SUCCESS;
}

int EspSocketTransport::sendFrame(const std::string& data) {
    // Encode message with length prefix
    std::string frame;
    int result = MessageFraming::encodeMessage(data, frame);
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    // Send frame
    const char* buffer = frame.c_str();
    size_t totalSize = frame.size();
    size_t sent = 0;
    
    while (sent < totalSize) {
        ssize_t result = ::send(socket_, buffer + sent, totalSize - sent, MSG_NOSIGNAL);
        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout or would block
                ESP_LOGW(TAG, "Send timeout");
                return TINYMCP_ERROR_TIMEOUT;
            } else if (errno == EPIPE || errno == ECONNRESET) {
                // Connection closed by peer
                ESP_LOGW(TAG, "Connection closed during send");
                connected_ = false;
                return TINYMCP_ERROR_TRANSPORT_FAILED;
            } else {
                ESP_LOGE(TAG, "Send error: %s", strerror(errno));
                return TINYMCP_ERROR_TRANSPORT_FAILED;
            }
        } else if (result == 0) {
            ESP_LOGW(TAG, "Connection closed by peer");
            connected_ = false;
            return TINYMCP_ERROR_TRANSPORT_FAILED;
        }
        
        sent += result;
    }
    
    return TINYMCP_SUCCESS;
}

int EspSocketTransport::receiveFrame(std::string& data, uint32_t timeoutMs) {
    data.clear();
    
    // First, read the 4-byte length header
    uint32_t messageLength;
    int result = receiveExact(&messageLength, sizeof(messageLength), timeoutMs);
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    // Convert from network byte order
    messageLength = ntohl(messageLength);
    
    // Validate message length
    if (messageLength == 0) {
        return TINYMCP_SUCCESS; // Empty message
    }
    
    if (messageLength > config_.maxMessageSize) {
        ESP_LOGW(TAG, "Message too large: %u bytes", messageLength);
        return TINYMCP_ERROR_MESSAGE_TOO_LARGE;
    }
    
    // Receive message data
    data.resize(messageLength);
    result = receiveExact(&data[0], messageLength, timeoutMs);
    if (result != TINYMCP_SUCCESS) {
        data.clear();
        return result;
    }
    
    return TINYMCP_SUCCESS;
}

int EspSocketTransport::receiveExact(void* buffer, size_t size, uint32_t timeoutMs) {
    char* ptr = static_cast<char*>(buffer);
    size_t received = 0;
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (received < size) {
        ssize_t result = recv(socket_, ptr + received, size - received, 0);
        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ESP_LOGW(TAG, "Receive timeout");
                return TINYMCP_ERROR_TIMEOUT;
            } else if (errno == ECONNRESET) {
                ESP_LOGW(TAG, "Connection reset by peer");
                connected_ = false;
                return TINYMCP_ERROR_TRANSPORT_FAILED;
            } else {
                ESP_LOGE(TAG, "Receive error: %s", strerror(errno));
                return TINYMCP_ERROR_TRANSPORT_FAILED;
            }
        } else if (result == 0) {
            ESP_LOGW(TAG, "Connection closed by peer");
            connected_ = false;
            return TINYMCP_ERROR_TRANSPORT_FAILED;
        }
        
        received += result;
    }
    
    return TINYMCP_SUCCESS;
}

std::string EspSocketTransport::formatClientAddress() const {
    return SocketUtils::formatAddress(clientAddr_);
}

int EspSocketTransport::getSocketError() const {
    return SocketUtils::getSocketError(socket_);
}

void EspSocketTransport::resetStats() {
    stats_ = TransportStats();
}

// EspSocketServer implementation
EspSocketServer::EspSocketServer(uint16_t port, const SocketTransportConfig& config) :
    config_(config), port_(port), maxConnections_(10), reuseAddress_(true),
    listenSocket_(-1), running_(false), activeConnections_(0) {
}

EspSocketServer::~EspSocketServer() {
    stop();
}

int EspSocketServer::start() {
    if (running_) {
        return TINYMCP_SUCCESS;
    }
    
    int result = createListenSocket();
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    result = bindSocket();
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    result = startListening();
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    running_ = true;
    ESP_LOGI(TAG, "Socket server started on port %d", port_);
    
    return TINYMCP_SUCCESS;
}

int EspSocketServer::stop() {
    if (!running_) {
        return TINYMCP_SUCCESS;
    }
    
    running_ = false;
    
    if (listenSocket_ >= 0) {
        ::close(listenSocket_);
        listenSocket_ = -1;
    }
    
    ESP_LOGI(TAG, "Socket server stopped");
    return TINYMCP_SUCCESS;
}

std::unique_ptr<EspSocketTransport> EspSocketServer::acceptConnection(uint32_t timeoutMs) {
    if (!running_ || listenSocket_ < 0) {
        return nullptr;
    }
    
    // Set socket to non-blocking for timeout
    if (timeoutMs > 0) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket_, &readSet);
        
        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        
        int selectResult = select(listenSocket_ + 1, &readSet, NULL, NULL, &timeout);
        if (selectResult <= 0) {
            return nullptr; // Timeout or error
        }
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    int clientSocket = accept(listenSocket_, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        ESP_LOGW(TAG, "Accept failed: %s", strerror(errno));
        stats_.acceptErrors++;
        return nullptr;
    }
    
    stats_.connectionsAccepted++;
    activeConnections_++;
    
    ESP_LOGI(TAG, "Accepted connection from %s", SocketUtils::formatAddress(clientAddr).c_str());
    
    return std::make_unique<EspSocketTransport>(clientSocket, config_);
}

int EspSocketServer::createListenSocket() {
    listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket_ < 0) {
        ESP_LOGE(TAG, "Failed to create listen socket: %s", strerror(errno));
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

int EspSocketServer::bindSocket() {
    if (reuseAddress_) {
        SocketUtils::setSocketReuseAddress(listenSocket_, true);
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);
    
    if (bind(listenSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        ESP_LOGE(TAG, "Bind failed on port %d: %s", port_, strerror(errno));
        ::close(listenSocket_);
        listenSocket_ = -1;
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

int EspSocketServer::startListening() {
    if (listen(listenSocket_, maxConnections_) < 0) {
        ESP_LOGE(TAG, "Listen failed: %s", strerror(errno));
        ::close(listenSocket_);
        listenSocket_ = -1;
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

// SocketUtils implementation
namespace SocketUtils {

std::string errorToString(int error) {
    return std::string(strerror(error));
}

int setSocketTimeout(int socket, uint32_t timeoutMs, bool receive) {
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    int option = receive ? SO_RCVTIMEO : SO_SNDTIMEO;
    if (setsockopt(socket, SOL_SOCKET, option, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "Failed to set socket timeout: %s", strerror(errno));
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

int setSocketKeepAlive(int socket, bool enable, uint32_t idle, uint32_t interval, uint32_t count) {
    int enableFlag = enable ? 1 : 0;
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &enableFlag, sizeof(enableFlag)) < 0) {
        ESP_LOGW(TAG, "Failed to set keep-alive: %s", strerror(errno));
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    if (enable) {
        // These options may not be available on all platforms
        #ifdef TCP_KEEPIDLE
        int idleTime = idle;
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idleTime, sizeof(idleTime));
        #endif
        
        #ifdef TCP_KEEPINTVL
        int intervalTime = interval;
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &intervalTime, sizeof(intervalTime));
        #endif
        
        #ifdef TCP_KEEPCNT
        int keepCount = count;
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));
        #endif
    }
    
    return TINYMCP_SUCCESS;
}

int setSocketNonBlocking(int socket, bool nonBlocking) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) {
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(socket, F_SETFL, flags) < 0) {
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

int setSocketReuseAddress(int socket, bool reuse) {
    int flag = reuse ? 1 : 0;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

std::string formatAddress(const struct sockaddr_in& addr) {
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
    return std::string(buffer) + ":" + std::to_string(ntohs(addr.sin_port));
}

int resolveHostname(const std::string& hostname, struct sockaddr_in& addr) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, hostname.c_str(), &addr.sin_addr) == 1) {
        return TINYMCP_SUCCESS; // Already an IP address
    }
    
    // For embedded systems, hostname resolution might be limited
    // This is a basic implementation
    ESP_LOGW(TAG, "Hostname resolution not fully implemented");
    return TINYMCP_ERROR_NOT_IMPLEMENTED;
}

bool isNetworkAvailable() {
    // Basic network availability check
    int testSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (testSocket < 0) {
        return false;
    }
    
    ::close(testSocket);
    return true;
}

int getSocketError(int socket) {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
        return errno;
    }
    return error;
}

} // namespace SocketUtils

// MessageFraming implementation
int MessageFraming::encodeMessage(const std::string& message, std::string& frame) {
    if (message.size() > MAX_MESSAGE_SIZE) {
        return TINYMCP_ERROR_MESSAGE_TOO_LARGE;
    }
    
    uint32_t length = htonl(static_cast<uint32_t>(message.size()));
    
    frame.clear();
    frame.reserve(HEADER_SIZE + message.size());
    frame.append(reinterpret_cast<const char*>(&length), HEADER_SIZE);
    frame.append(message);
    
    return TINYMCP_SUCCESS;
}

int MessageFraming::decodeMessage(const std::string& frame, std::string& message) {
    if (frame.size() < HEADER_SIZE) {
        return TINYMCP_ERROR_INVALID_MESSAGE;
    }
    
    uint32_t length;
    memcpy(&length, frame.data(), HEADER_SIZE);
    length = ntohl(length);
    
    if (length > MAX_MESSAGE_SIZE) {
        return TINYMCP_ERROR_MESSAGE_TOO_LARGE;
    }
    
    if (frame.size() != HEADER_SIZE + length) {
        return TINYMCP_ERROR_INVALID_MESSAGE;
    }
    
    message = frame.substr(HEADER_SIZE, length);
    return TINYMCP_SUCCESS;
}

int MessageFraming::sendFramedMessage(int socket, const std::string& message, uint32_t timeoutMs) {
    std::string frame;
    int result = encodeMessage(message, frame);
    if (result != TINYMCP_SUCCESS) {
        return result;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Send frame
    size_t sent = 0;
    while (sent < frame.size()) {
        ssize_t result = send(socket, frame.data() + sent, frame.size() - sent, MSG_NOSIGNAL);
        if (result < 0) {
            return TINYMCP_ERROR_TRANSPORT_FAILED;
        }
        sent += result;
    }
    
    return TINYMCP_SUCCESS;
}

int MessageFraming::receiveFramedMessage(int socket, std::string& message, uint32_t timeoutMs) {
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Receive header
    uint32_t length;
    ssize_t received = recv(socket, &length, HEADER_SIZE, MSG_WAITALL);
    if (received != HEADER_SIZE) {
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    length = ntohl(length);
    if (length > MAX_MESSAGE_SIZE) {
        return TINYMCP_ERROR_MESSAGE_TOO_LARGE;
    }
    
    // Receive message
    message.resize(length);
    received = recv(socket, &message[0], length, MSG_WAITALL);
    if (received != static_cast<ssize_t>(length)) {
        return TINYMCP_ERROR_TRANSPORT_FAILED;
    }
    
    return TINYMCP_SUCCESS;
}

} // namespace tinymcp