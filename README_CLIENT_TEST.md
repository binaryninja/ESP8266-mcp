# ESP8266 MCP Client Test

This directory contains a comprehensive Python-based test client for the ESP8266 MCP (Machine Control Protocol) server.

## Overview

The test client validates all MCP protocol features including:
- 🔗 Connection establishment
- 🚀 Session initialization
- 🏓 Ping/connectivity testing
- 🔍 Tool discovery
- 🛠️ Tool execution (echo, GPIO control)
- ❌ Error handling
- 📊 Comprehensive reporting

## Quick Start

### 1. Flash the ESP8266

First, build and flash the MCP server to your ESP8266:

```bash
source ~/esp/ESP8266_RTOS_SDK/export.sh
idf.py build flash monitor
```

Note the IP address assigned to your ESP8266 (shown in the serial monitor).

### 2. Run the Test Client

```bash
# Basic comprehensive test
python test_mcp_client.py 192.168.1.100

# Specify custom port
python test_mcp_client.py 192.168.1.100 8080

# Interactive mode
python test_mcp_client.py 192.168.1.100 --interactive

# Quick connectivity test
python test_mcp_client.py 192.168.1.100 --ping-only
```

## Usage Examples

### Comprehensive Test (Default)
```bash
python test_mcp_client.py 192.168.1.100
```

This runs all tests automatically:
- ✅ Connection test
- ✅ MCP initialization
- ✅ Ping test
- ✅ Tool discovery
- ✅ Echo tool tests (multiple messages)
- ✅ GPIO control tests (multiple pins/states)
- ✅ Error handling tests
- ✅ Invalid parameter tests

### Interactive Mode
```bash
python test_mcp_client.py 192.168.1.100 --interactive
```

Available interactive commands:
- `ping` - Test server connectivity
- `list` - List available tools
- `echo <message>` - Test echo tool
- `gpio <pin> <state>` - Control GPIO pin (state: high/low)
- `test` - Run comprehensive test suite
- `quit` - Exit interactive mode

Example interactive session:
```
> ping
🏓 Sending ping...
✅ Ping successful!

> echo Hello ESP8266!
🔧 Calling tool 'echo' with arguments: {'text': 'Hello ESP8266!'}
✅ Tool 'echo' executed successfully!
📄 Response content:
   📝 Echo: Hello ESP8266!

> gpio 2 high
🔧 Calling tool 'gpio_control' with arguments: {'pin': 2, 'state': 'high'}
✅ Tool 'gpio_control' executed successfully!
📄 Response content:
   📝 GPIO pin 2 set to high

> gpio 2 low
🔧 Calling tool 'gpio_control' with arguments: {'pin': 2, 'state': 'low'}
✅ Tool 'gpio_control' executed successfully!
📄 Response content:
   📝 GPIO pin 2 set to low
```

### Quick Connectivity Test
```bash
python test_mcp_client.py 192.168.1.100 --ping-only
```

## Test Features

### 🔊 Echo Tool Tests
Tests the echo functionality with various inputs:
- Simple messages
- Special characters
- Multi-line text
- Unicode characters

### 🔌 GPIO Control Tests
Tests GPIO control with different pins and states:
- Pin 2 (built-in LED on most ESP8266 boards)
- Pin 16 (common GPIO pin)
- Pin 0 (GPIO 0)
- High/Low state switching

### ❌ Error Handling Tests
Validates proper error responses for:
- Invalid tool names
- Missing required parameters
- Invalid parameter values

## Expected Output

### Successful Test Run
```
🤖 ESP8266 MCP Client Test Script
============================================================
Target: 192.168.1.100:8080
============================================================

🧪 Starting comprehensive MCP test suite...
============================================================

📡 TEST 1: Connection
🔗 Connecting to MCP server at 192.168.1.100:8080...
✅ Connected successfully!

🚀 TEST 2: Initialization
🚀 Initializing MCP session...
📤 Sending: {"jsonrpc": "2.0", "id": "1", "method": "initialize", "params": {...}}
📥 Received: {"jsonrpc":"2.0","id":"1","result":{...}}
✅ Server initialized!
   📋 Server: ESP8266-MCP v1.0.0
   🔧 Protocol: 2024-11-05
   🛠️  Tools supported: false

[... additional test output ...]

============================================================
🏁 COMPREHENSIVE TEST RESULTS
============================================================
✅ PASS - Connection
✅ PASS - Initialization
✅ PASS - Ping
✅ PASS - Tool Discovery
✅ PASS - Echo Tool
✅ PASS - GPIO Tool
✅ PASS - Error Handling
✅ PASS - Invalid Parameters

📊 Overall Results: 8/8 tests passed
🎯 Success Rate: 100.0%
🎉 All tests passed! ESP8266 MCP server is working perfectly!
```

## Troubleshooting

### Connection Issues
- ✅ Verify ESP8266 is connected to WiFi
- ✅ Check IP address (use `idf.py monitor` to see boot logs)
- ✅ Ensure firewall allows port 8080
- ✅ Test basic connectivity: `ping 192.168.1.100`

### Server Not Responding
- ✅ Check serial monitor for ESP8266 errors
- ✅ Verify MCP server is running (should show "MCP server listening...")
- ✅ Try restarting the ESP8266
- ✅ Check memory usage (low memory can cause issues)

### Tool Execution Failures
- ✅ Ensure server initialization completed successfully
- ✅ Check parameter names and types match tool schema
- ✅ Verify GPIO pins are valid for your ESP8266 board

## MCP Protocol Details

The client implements the MCP (Machine Control Protocol) specification:

### JSON-RPC 2.0 Format
```json
{
  "jsonrpc": "2.0",
  "id": "request_id",
  "method": "method_name",
  "params": { ... }
}
```

### Supported Methods
- `initialize` - Start MCP session
- `ping` - Test connectivity
- `tools/list` - Discover available tools
- `tools/call` - Execute a tool

### Server Capabilities
The ESP8266 MCP server provides:
- 📋 Server info (name, version)
- 🛠️ Tool management
- 🔧 JSON-RPC 2.0 protocol
- ❌ Error handling
- 📡 Socket-based transport

## Hardware Notes

### GPIO Pin Reference (NodeMCU/Wemos D1 Mini)
- Pin 0 (D3) - GPIO 0
- Pin 2 (D4) - GPIO 2 (built-in LED, inverted)
- Pin 4 (D2) - GPIO 4
- Pin 5 (D1) - GPIO 5
- Pin 12 (D6) - GPIO 12
- Pin 13 (D7) - GPIO 13
- Pin 14 (D5) - GPIO 14
- Pin 15 (D8) - GPIO 15
- Pin 16 (D0) - GPIO 16

### LED Testing
Most ESP8266 boards have a built-in LED on GPIO 2 that is **inverted**:
- `gpio 2 low` = LED ON
- `gpio 2 high` = LED OFF

## Development

### Adding New Tests
To add new test cases, extend the `MCPClient` class:

```python
def test_new_feature(self) -> bool:
    """Test a new feature."""
    # Implementation here
    return success
```

### Protocol Extensions
The client can be extended to test additional MCP features:
- Resource management
- Prompts
- Notifications
- Sampling

## License

This test client is provided as part of the ESP8266-MCP project.