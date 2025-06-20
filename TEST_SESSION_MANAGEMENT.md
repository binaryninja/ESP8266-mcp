# ESP8266 Session Management System - Test Suite Documentation

## Overview

This document describes the comprehensive test suite for the ESP8266 FreeRTOS Session Management System. The test suite validates all aspects of the session-based MCP (Model Context Protocol) implementation including session lifecycle, transport layer, tool execution, async tasks, and error handling.

## Test Suite Components

### 1. On-Device Test Framework (`test_session_system.cpp`)
- **Purpose**: Unit tests that run directly on the ESP8266
- **Features**: 
  - Session state transition testing
  - Memory usage validation
  - Async task execution testing
  - Tool registry validation
  - Mock transport for isolated testing
- **Monitoring**: Uses ESP_LOG for serial console output

### 2. Session Management Test Client (`test_session_management.py`)
- **Purpose**: Comprehensive integration testing of session management
- **Test Categories**:
  - Basic connection and initialization
  - Concurrent session handling
  - Session persistence and recovery
  - Error handling scenarios
  - Performance metrics
  - Message framing protocol

### 3. Tool Testing Client (`test_session_tools.py`)
- **Purpose**: Detailed testing of all built-in tools
- **Supported Tools**:
  - Echo tool (basic functionality)
  - System info tool (hardware information)
  - GPIO control tool (pin manipulation)
  - Network scanner tool (async WiFi scanning)
  - File system tool (file operations)
  - I2C scanner tool (hardware bus scanning)
  - Long running task tool (progress reporting)

### 4. Comprehensive Test Script (`test_session_comprehensive.sh`)
- **Purpose**: Automated build, flash, and test execution
- **Features**:
  - Automated firmware building and flashing
  - Serial monitoring and logging
  - Multiple test suite execution
  - Report generation
  - Error handling and cleanup

## Quick Start

### Prerequisites

1. **ESP-IDF Environment**:
   ```bash
   source $IDF_PATH/export.sh
   ```

2. **Python Dependencies**:
   ```bash
   pip3 install pyserial
   ```

3. **Hardware Setup**:
   - ESP8266 connected via USB (typically `/dev/ttyUSB0`)
   - ESP8266 connected to WiFi network
   - Note the ESP8266's IP address

### Running Tests

#### Option 1: Full Automated Test Suite
```bash
# Complete build-flash-test cycle
./test_session_comprehensive.sh -i 192.168.1.100

# With verbose output
./test_session_comprehensive.sh -i 192.168.1.100 --verbose

# Test specific tool only
./test_session_comprehensive.sh -i 192.168.1.100 --tool echo
```

#### Option 2: Individual Test Components

**Build and Flash Only:**
```bash
./test_session_comprehensive.sh --flash-only
```

**Session Management Tests Only:**
```bash
python3 test_session_management.py 192.168.1.100
```

**Tool Tests Only:**
```bash
python3 test_session_tools.py 192.168.1.100
```

**Test Only (Skip Build/Flash):**
```bash
./test_session_comprehensive.sh --test-only -i 192.168.1.100
```

#### Option 3: Manual Testing

**Serial Monitoring:**
```bash
python3 test_serial.py /dev/ttyUSB0
```

**Original MCP Client:**
```bash
python3 test_mcp_client.py 192.168.1.100
```

## Test Configuration

### Command Line Options

#### `test_session_comprehensive.sh`
```
-h, --help              Show help message
-i, --ip IP_ADDRESS     ESP8266 IP address for testing
-p, --port SERIAL_PORT  Serial port for flashing (default: /dev/ttyUSB0)
-m, --mcp-port PORT     MCP server port (default: 8080)
-v, --verbose           Enable verbose output
-f, --flash-only        Only build and flash, don't run tests
-t, --test-only         Only run tests (skip build/flash)
-T, --tool TOOL_NAME    Test specific tool only
-s, --sessions N        Number of concurrent sessions to test (default: 3)
--timeout SECONDS       Test timeout in seconds (default: 300)
```

#### `test_session_management.py`
```
positional arguments:
  host                  ESP8266 IP address

optional arguments:
  --port PORT           Server port (default: 8080)
  --sessions N          Number of concurrent sessions (default: 3)
  --verbose, -v         Verbose output
```

#### `test_session_tools.py`
```
positional arguments:
  host                  ESP8266 IP address

optional arguments:
  --port PORT           Server port (default: 8080)
  --tool TOOL_NAME      Test specific tool only
  --verbose, -v         Verbose output
```

### Configuration File (`test_config.json`)

The test suite uses a comprehensive JSON configuration file that defines:
- Default settings (IP, ports, timeouts)
- Test suite configurations
- Hardware profiles
- Validation rules
- Debug options

Key configuration sections:
- `default_settings`: Basic connection and timeout settings
- `test_suites`: Detailed test configurations for each suite
- `hardware_profiles`: ESP8266 variant-specific settings
- `validation_rules`: Performance and reliability thresholds

## Test Results and Reporting

### Console Output

Tests provide real-time console output with:
- ✅ Pass/❌ Fail indicators
- Execution timing
- Error messages and details
- Progress indicators for long-running tests

### Report Generation

The comprehensive test script generates:
- **Markdown Report**: Detailed test results with system info
- **Serial Logs**: Complete ESP8266 serial output
- **Build Logs**: Compilation and flash logs
- **Test Artifacts**: Individual test outputs

### Success Criteria

Tests are considered successful when:
- All session state transitions work correctly
- Multiple concurrent sessions can be established
- All tools execute without errors
- Memory usage remains within acceptable limits
- Response times meet performance thresholds
- Error handling works as expected

## Understanding Test Results

### Session Management Tests

1. **Basic Connection**: Validates TCP socket connection to MCP server
2. **Session Initialization**: Tests MCP protocol handshake
3. **Tool Discovery**: Verifies tool listing functionality
4. **Concurrent Sessions**: Tests multiple simultaneous connections
5. **Async Tool Execution**: Validates asynchronous task processing
6. **Error Handling**: Tests error scenarios and recovery
7. **Session Persistence**: Tests reconnection after disconnection
8. **Performance Metrics**: Measures latency and throughput
9. **Message Framing**: Validates message protocol integrity

### Tool Tests

Each tool is tested for:
- **Functionality**: Basic operation with valid parameters
- **Parameter Validation**: Response to invalid/missing parameters
- **Error Handling**: Graceful failure scenarios
- **Progress Reporting**: Async progress notifications (where applicable)
- **Resource Management**: Memory and resource cleanup

### On-Device Tests

The on-device test framework validates:
- Session state machine transitions
- Memory leak detection
- Task execution and cleanup
- Tool registry operations
- Transport layer functionality

## Troubleshooting

### Common Issues

#### Connection Problems
```
❌ Connection failed: [Errno 111] Connection refused
```
**Solutions**:
- Verify ESP8266 IP address
- Check WiFi connection
- Ensure MCP server is running
- Check firewall settings

#### Flash Problems
```
❌ Flash failed. Check flash.log for details.
```
**Solutions**:
- Check USB cable connection
- Verify serial port permissions: `sudo chmod 666 /dev/ttyUSB0`
- Try different serial port
- Reset ESP8266 manually

#### Test Timeouts
```
⏰ Test timeout after 300 seconds
```
**Solutions**:
- Increase timeout with `--timeout` option
- Check ESP8266 performance
- Reduce concurrent session count
- Test individual components

#### Memory Issues
Tests may fail if ESP8266 runs out of memory:
- Monitor serial output for heap warnings
- Reduce concurrent sessions
- Check for memory leaks in custom code

### Debug Options

#### Verbose Mode
Add `-v` or `--verbose` to any test command for detailed output:
```bash
./test_session_comprehensive.sh -i 192.168.1.100 --verbose
```

#### Serial Monitoring
Monitor ESP8266 serial output in real-time:
```bash
python3 test_serial.py /dev/ttyUSB0
```

#### Individual Test Components
Run specific test suites to isolate issues:
```bash
# Test only session management
python3 test_session_management.py 192.168.1.100 --verbose

# Test only echo tool
python3 test_session_tools.py 192.168.1.100 --tool echo --verbose
```

## Extending the Test Suite

### Adding New Tool Tests

1. **Add Tool Test Function**:
   ```python
   def test_my_custom_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
       response, progress = self.client.call_tool("my_tool", {"param": "value"})
       # Validate response
       return success, result_data, progress
   ```

2. **Register Test**:
   ```python
   tool_tests = {
       "my_tool": self.test_my_custom_tool,
       # ... existing tools
   }
   ```

### Adding New Session Tests

1. **Add Test Method**:
   ```python
   def test_my_feature(self) -> Tuple[bool, Dict[str, Any]]:
       # Test implementation
       return success, details
   ```

2. **Add to Test Suite**:
   ```python
   tests = [
       ("My Feature Test", self.test_my_feature),
       # ... existing tests
   ]
   ```

### Configuring Hardware Profiles

Edit `test_config.json` to add new hardware profiles:
```json
{
  "hardware_profiles": {
    "my_esp8266_board": {
      "name": "My Custom ESP8266",
      "flash_size": "4MB",
      "ram_size": "80KB",
      "expected_free_heap": 30000,
      "gpio_pins": [0, 2, 4, 5, 12, 13, 14, 15, 16]
    }
  }
}
```

## Performance Benchmarks

### Expected Performance Characteristics

- **Connection Time**: < 2 seconds
- **Session Initialization**: < 3 seconds
- **Tool Response Time**: < 5 seconds (simple tools)
- **Async Tool Response**: < 30 seconds (complex tools)
- **Concurrent Sessions**: 3+ simultaneous connections
- **Memory Usage**: < 70% of available heap
- **Throughput**: 10+ messages/second

### Memory Usage

Typical memory footprint:
- **Base System**: ~13KB
- **Per Session**: ~9KB additional
- **Free Heap**: 25KB+ remaining (on 4MB ESP8266)

## Continuous Integration

The test suite is designed for CI/CD integration:

### GitHub Actions Example
```yaml
name: ESP8266 Session Management Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
      - name: Run Tests
        run: |
          ./test_session_comprehensive.sh --flash-only
          # Additional CI-specific test commands
```

### Test Reports

The test suite generates CI-friendly reports:
- JUnit XML format support (configurable)
- JSON result format for parsing
- Return codes for CI pass/fail detection

## Contributing

When contributing to the test suite:

1. **Follow Naming Conventions**: Use descriptive test names
2. **Add Documentation**: Document new test functions
3. **Handle Errors Gracefully**: Tests should not crash on failures
4. **Include Cleanup**: Ensure proper resource cleanup
5. **Test Edge Cases**: Include boundary and error conditions
6. **Update Configuration**: Add new tests to `test_config.json`

## Support

For issues with the test suite:

1. **Check Prerequisites**: Ensure ESP-IDF and Python setup
2. **Review Console Output**: Look for specific error messages
3. **Check Hardware**: Verify ESP8266 connections and power
4. **Isolate Issues**: Use individual test components
5. **Enable Debug Output**: Use verbose mode for detailed logs

## License

This test suite is part of the ESP8266-MCP project and follows the same license terms.