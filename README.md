# ESP8266-MCP

A practical implementation of TinyMCP (Model Context Protocol) running on ESP8266 using ESP8266-RTOS-SDK. This project demonstrates how to create a lightweight MCP server on embedded hardware that can communicate with AI models over WiFi.

## Overview

This project implements a "from-zero-to-blinky" style MCP server for ESP8266 that:
- Runs TinyMCP server over WiFi TCP connection
- Provides example tools (echo, GPIO control)
- Uses minimal resources (~128 kB flash, ~28 kB RAM peak)
- Supports standard MCP protocol for AI model integration

## Architecture

| Stack Layer | Component | Description |
|-------------|-----------|-------------|
| Application | **TinyMCP** (C++17) | Lightweight MCP server implementation |
| Transport | **EspSocketTransport** | Custom WiFi TCP transport layer |
| OS / HAL | **ESP8266-RTOS-SDK** | FreeRTOS + lwIP networking stack |
| Toolchain | **xtensa-lx106-elf-gcc** | C++17 capable cross-compiler |

## 🚀 Quick Start

**New to this project? Start here:**

1. **[Quick Test Guide](QUICK_TEST_GUIDE.md)** - Get up and running in 5 minutes
2. **[Complete Test Documentation](TEST_SESSION_MANAGEMENT.md)** - Comprehensive test suite guide

### Basic Setup

⚠️ **IMPORTANT**: You must set up the ESP8266-RTOS-SDK environment before running any `idf.py` commands, or you'll get "command not found" errors.

```bash
# 1. Set up ESP-IDF environment (REQUIRED for every terminal session)
export IDF_PATH=/home/$USER/esp/ESP8266_RTOS_SDK
source $IDF_PATH/export.sh

# 2. Build and flash
idf.py build
idf.py -p /dev/ttyUSB0 flash

# 3. Find ESP8266 IP address and test
python3 test_mcp_client.py 192.168.x.x
```

💡 **Pro Tip**: Add the environment setup to your `~/.bashrc` to avoid repeating it:
```bash
echo 'export IDF_PATH=/home/$USER/esp/ESP8266_RTOS_SDK' >> ~/.bashrc
echo 'alias esp-env="source $IDF_PATH/export.sh"' >> ~/.bashrc
```
Then just run `esp-env` before each development session.

**Need help?** Check [QUICK_TEST_GUIDE.md](QUICK_TEST_GUIDE.md) for troubleshooting.

## Project Structure

```
ESP8266-mcp/
├── CMakeLists.txt              # Top-level build configuration
├── components/
│   └── tinymcp/                # TinyMCP library (git submodule)
│       ├── CMakeLists.txt      # Component build configuration
│       ├── EspSocketTransport.h/cpp  # Custom WiFi transport
│       ├── MCPServer.h/cpp     # MCP protocol implementation
│       └── Source/             # TinyMCP core library
└── main/
    ├── app_main.cpp            # Main application code
    └── CMakeLists.txt          # Main component configuration
```

## Prerequisites

1. **ESP8266-RTOS-SDK** (v5.4.x or later)
2. **xtensa-lx106-elf-gcc** toolchain (13.2 or later)
3. **Git** (for submodules)

### Installing ESP8266-RTOS-SDK

```bash
# Install prerequisites (Ubuntu/Debian)
sudo apt-get install gcc git wget make libncurses-dev flex bison gperf python3 python3-pip python3-setuptools python3-serial python3-cryptography python3-future python3-pyparsing python3-pyelftools cmake ninja-build ccache

# Clone ESP8266-RTOS-SDK
git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
cd ESP8266_RTOS_SDK
./install.sh

# Set up environment (add to ~/.bashrc)
export IDF_PATH=/path/to/ESP8266_RTOS_SDK
```

## Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/binaryninja/ESP8266-mcp.git
cd ESP8266-mcp
git submodule update --init --recursive
```

### 2. Configure WiFi Credentials

Edit `main/app_main.cpp` and update the WiFi configuration:

```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
```

### 3. Build and Flash

```bash
# Set up ESP8266-RTOS-SDK environment
source $IDF_PATH/export.sh

# Configure for ESP8266
idf.py set-target esp8266

# Optional: Configure advanced settings
idf.py menuconfig
# Recommended: Component config → C++ exceptions → Disable

# Build the project
idf.py build

# Flash to device (replace /dev/ttyUSB0 with your serial port)
idf.py -p /dev/ttyUSB0 flash monitor
```

## Usage

### Connecting to the MCP Server

Once flashed and running, the ESP8266 will:

1. Connect to your WiFi network
2. Start an MCP server on port 8080
3. Display the IP address in the serial monitor

### Testing with the Python Client

The project includes a comprehensive Python-based test client that validates all MCP features:

#### Quick Start Testing

```bash
# Run comprehensive test suite
python3 test_mcp_client.py 192.168.1.100

# Interactive testing mode
python3 test_mcp_client.py 192.168.1.100 --interactive

# Quick connectivity test
python3 test_mcp_client.py 192.168.1.100 --ping-only

# Use the quick setup script
./quick_test.sh
```

#### Test Features

The Python client tests:
- 🔗 **Connection establishment** - TCP socket connectivity
- 🚀 **MCP initialization** - Protocol handshake and capabilities
- 🏓 **Ping/connectivity** - Server responsiveness
- 🔍 **Tool discovery** - Available tools enumeration
- 🛠️ **Tool execution** - Echo and GPIO control tools
- ❌ **Error handling** - Invalid requests and parameters
- 📊 **Comprehensive reporting** - Pass/fail status with details

#### Interactive Mode Commands

```
> ping                 # Test server connectivity
> list                 # List available tools
> echo Hello World!    # Test echo tool
> gpio 2 high          # Set GPIO pin 2 high
> gpio 2 low           # Set GPIO pin 2 low
> test                 # Run full test suite
> quit                 # Exit interactive mode
```

#### Expected Output

```
🤖 ESP8266 MCP Client Test Script
============================================================
Target: 192.168.1.100:8080
============================================================

🧪 Starting comprehensive MCP test suite...

✅ PASS - Connection
✅ PASS - Initializationent:

```bash
# Connect via telnet
telnet <ESP8266_IP_ADDRESS> 8080

# Send initialization request
{"jsonrpc":"2.0","id":"1","method":"initialize","params":{"protocolVersion":"2024-11-05","clientInfo":{"name":"test-client","version":"1.0.0"},"capabilities":{}}}

# List available tools
{"jsonrpc":"2.0","id":"2","method":"tools/list","params":{}}

# Call the echo tool
{"jsonrpc":"2.0","id":"3","method":"tools/call","params":{"name":"echo","arguments":{"text":"Hello ESP8266!"}}}

# Call the GPIO control tool
{"jsonrpc":"2.0","id":"4","method":"tools/call","params":{"name":"gpio_control","arguments":{"pin":2,"state":"high"}}}
```

### Available Tools

The server includes two example tools:

1. **echo**: Echoes back the provided text
   - Input: `{"text": "your message"}`
   - Output: Echoed message

2. **gpio_control**: Controls GPIO pins (simulation)
   - Input: `{"pin": 2, "state": "high|low"}`
   - Output: Confirmation message

## Configuration Options

### Memory Optimization

For better performance on ESP8266's limited resources:

```bash
idf.py menuconfig
# Component config → ESP8266-specific → Memory
# - Set "Max number of open sockets" to 4
# - Disable unused networking features

# Component config → FreeRTOS
# - Optimize for size (-Os)
# - Reduce task stack sizes where possible
```

### WiFi Performance

For better network performance:

```cpp
// In app_main.cpp, the code already includes:
ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // Disable power save
```

## Extending the Server

### Adding New Tools

1. Add tool definition in `MCPServer::handleToolsList()`
2. Add tool implementation in `MCPServer::handleToolsCall()`

Example:

```cpp
// In handleToolsList():
Json::Value newTool;
newTool["name"] = "my_tool";
newTool["description"] = "My custom tool";
// ... define input schema
tools.append(newTool);

// In handleToolsCall():
else if (toolName == "my_tool") {
    // Implement your tool logic here
}
```

### Adding Resources or Prompts

The current implementation focuses on Tools. To add Resources or Prompts:

1. Add capability in `handleInitialize()`
2. Implement `handleResourcesList()` and/or `handlePromptsList()`
3. Add corresponding handlers in `processMessage()`

## Troubleshooting

### Common Issues

1. **"idf.py: command not found" Error**
   - **Cause**: ESP8266-RTOS-SDK environment not loaded
   - **Solution**: Run these commands before building:
     ```bash
     export IDF_PATH=/home/$USER/esp/ESP8266_RTOS_SDK
     source $IDF_PATH/export.sh
     ```
   - **Permanent Fix**: Add to your `~/.bashrc`:
     ```bash
     export IDF_PATH=/home/$USER/esp/ESP8266_RTOS_SDK
     alias esp-env='source $IDF_PATH/export.sh'
     ```
     Then run `esp-env` before each development session

2. **WiFi Connection Failed**
   - Check SSID/password in `app_main.cpp`
   - Verify 2.4GHz network (ESP8266 doesn't support 5GHz)

3. **Build Errors**
   - Ensure ESP8266-RTOS-SDK is properly installed
   - Check that `IDF_PATH` is set correctly
   - Run `git submodule update --init --recursive`

4. **Memory Issues**
   - Enable compiler optimizations (`-Os`)
   - Reduce task stack sizes
   - Disable unused ESP8266-RTOS-SDK components

5. **Connection Timeouts**
   - Check firewall settings
   - Verify ESP8266 and client are on same network
   - Try increasing socket timeouts

### Debug Logging

Enable verbose logging:

```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

Or modify log levels in code:

```cpp
esp_log_level_set("MCPServer", ESP_LOG_DEBUG);
esp_log_level_set("EspSocketTransport", ESP_LOG_DEBUG);
```

## Performance Characteristics

- **Flash Usage**: ~128 kB
- **RAM Usage**: ~28 kB peak
- **Response Time**: < 100ms for simple tools
- **Concurrent Connections**: 1 (single-threaded)
- **WiFi Throughput**: Limited by ESP8266 hardware (~1 Mbps effective)

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## References

- [Model Context Protocol Specification](https://modelcontextprotocol.io/specification/2024-11-05/index)
- [TinyMCP Library](https://github.com/Qihoo360/TinyMCP)
- [ESP8266-RTOS-SDK Documentation](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/)
- [ESP8266 Hardware Reference](https://www.espressif.com/en/products/socs/esp8266)

## Acknowledgments

- Qihoo360 for the TinyMCP library
- Espressif for the ESP8266-RTOS-SDK
- The Model Context Protocol community
