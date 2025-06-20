#pragma once

#include <string>

namespace tinymcp {

class Transport {
public:
    virtual ~Transport() = default;
    virtual bool read(std::string &buffer) = 0;
    virtual bool write(const std::string &buffer) = 0;
    virtual bool isConnected() const = 0;
    virtual void close() = 0;
};

class EspSocketTransport : public Transport {
public:
    explicit EspSocketTransport(int sock);
    ~EspSocketTransport() override;

    // Delete copy constructor and assignment operator
    EspSocketTransport(const EspSocketTransport&) = delete;
    EspSocketTransport& operator=(const EspSocketTransport&) = delete;

    bool read(std::string &buffer) override;
    bool write(const std::string &buffer) override;
    bool isConnected() const override;
    void close() override;

private:
    int sock_;
    std::string read_buffer_;  // Buffer for incomplete messages
};

} // namespace tinymcp
