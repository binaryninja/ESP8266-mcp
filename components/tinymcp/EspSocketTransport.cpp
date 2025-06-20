#include "EspSocketTransport.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <errno.h>

static const char *TAG = "EspSocketTransport";

namespace tinymcp {

EspSocketTransport::EspSocketTransport(int sock) : sock_(sock), read_buffer_("") {
    ESP_LOGI(TAG, "EspSocketTransport created with socket %d", sock_);
}

EspSocketTransport::~EspSocketTransport() {
    if (sock_ >= 0) {
        ESP_LOGI(TAG, "Closing socket %d", sock_);
        ::close(sock_);
        sock_ = -1;
    }
}

bool EspSocketTransport::read(std::string &buffer) {
    if (sock_ < 0) {
        ESP_LOGE(TAG, "Socket is invalid");
        return false;
    }

    // Buffer size limit to prevent DoS attacks (16KB should be enough for MCP messages)
    const size_t MAX_BUFFER_SIZE = 16384;

    // First check if we have a complete message in our buffer
    size_t newline_pos = read_buffer_.find('\n');
    if (newline_pos != std::string::npos) {
        // Extract complete message (excluding newline)
        buffer = read_buffer_.substr(0, newline_pos);
        // Remove message from buffer (including newline)
        read_buffer_.erase(0, newline_pos + 1);
        ESP_LOGD(TAG, "Returning buffered message: %s", buffer.c_str());
        return true;
    }

    // Check buffer size limit
    if (read_buffer_.size() >= MAX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Read buffer overflow, clearing buffer");
        read_buffer_.clear();
        buffer.clear();
        return false;
    }

    // No complete message in buffer, try to read more data
    char tmp[512];
    int r = ::recv(sock_, tmp, sizeof(tmp) - 1, MSG_DONTWAIT);

    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now, but socket is still connected
            buffer.clear();
            return true;
        }
        ESP_LOGE(TAG, "Socket read failed: errno %d", errno);
        return false;
    } else if (r == 0) {
        ESP_LOGI(TAG, "Socket closed by peer");
        return false;
    }

    tmp[r] = '\0';  // Null terminate for safety

    // Check if adding this data would exceed buffer limit
    if (read_buffer_.size() + r > MAX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Adding data would exceed buffer limit, clearing buffer");
        read_buffer_.clear();
        buffer.clear();
        return false;
    }

    read_buffer_.append(tmp, r);

    ESP_LOGD(TAG, "Read %d bytes from socket, buffer size: %d", r, read_buffer_.size());

    // Check again for complete message after adding new data
    newline_pos = read_buffer_.find('\n');
    if (newline_pos != std::string::npos) {
        buffer = read_buffer_.substr(0, newline_pos);
        read_buffer_.erase(0, newline_pos + 1);
        ESP_LOGD(TAG, "Returning new message: %s", buffer.c_str());
        return true;
    }

    // Still no complete message
    buffer.clear();
    return true;
}

bool EspSocketTransport::write(const std::string &buffer) {
    if (sock_ < 0) {
        ESP_LOGE(TAG, "Socket is invalid");
        return false;
    }

    if (buffer.empty()) {
        ESP_LOGW(TAG, "Attempted to write empty buffer");
        return true;
    }

    int total_sent = 0;
    int remaining = buffer.size();
    const char* data = buffer.data();

    while (remaining > 0) {
        int sent = ::send(sock_, data + total_sent, remaining, 0);

        if (sent < 0) {
            ESP_LOGE(TAG, "Socket write failed: errno %d", errno);
            return false;
        } else if (sent == 0) {
            ESP_LOGE(TAG, "Socket write returned 0, connection may be closed");
            return false;
        }

        total_sent += sent;
        remaining -= sent;
    }

    ESP_LOGD(TAG, "Wrote %d bytes to socket", total_sent);
    return true;
}

bool EspSocketTransport::isConnected() const {
    if (sock_ < 0) {
        return false;
    }

    // Use SO_ERROR to check if socket has errors
    int error = 0;
    socklen_t len = sizeof(error);
    int result = getsockopt(sock_, SOL_SOCKET, SO_ERROR, &error, &len);

    if (result != 0) {
        ESP_LOGD(TAG, "getsockopt failed: errno %d", errno);
        return false;
    }

    return error == 0;
}

void EspSocketTransport::close() {
    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
        ESP_LOGI(TAG, "Socket closed manually");
    }
    read_buffer_.clear();
}

} // namespace tinymcp
