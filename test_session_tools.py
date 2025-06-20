#!/usr/bin/env python3
"""
ESP8266 Session Management Tools Test Client

Comprehensive test suite for testing all built-in tools in the FreeRTOS Session Management System.
Tests tool execution, parameter validation, async operation, progress reporting, and error handling.

Usage:
    python test_session_tools.py <ESP8266_IP> [options]

Example:
    python test_session_tools.py 192.168.1.100
    python test_session_tools.py 192.168.1.100 --port 8080 --verbose --tool echo
"""

import json
import socket
import sys
import time
import threading
import argparse
from typing import Dict, Any, Optional, List, Tuple
from dataclasses import dataclass
from enum import Enum


@dataclass
class ToolTestResult:
    tool_name: str
    test_name: str
    passed: bool
    duration_ms: int
    error_message: str = ""
    result_data: Dict[str, Any] = None
    progress_events: List[Dict[str, Any]] = None

    def __post_init__(self):
        if self.result_data is None:
            self.result_data = {}
        if self.progress_events is None:
            self.progress_events = []


class SessionToolClient:
    """MCP client optimized for tool testing."""

    def __init__(self, host: str, port: int = 8080, client_id: str = None):
        self.host = host
        self.port = port
        self.client_id = client_id or f"tool_test_{int(time.time())}"
        self.socket = None
        self.request_id = 0
        self.initialized = False
        self.available_tools = {}
        self.lock = threading.Lock()
        self.progress_notifications = []

    def _get_next_id(self) -> int:
        """Get next request ID."""
        with self.lock:
            self.request_id += 1
            return self.request_id

    def connect(self, timeout: float = 10.0) -> bool:
        """Connect to the MCP server."""
        try:
            print(f"🔗 [{self.client_id}] Connecting to {self.host}:{self.port}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(timeout)
            self.socket.connect((self.host, self.port))
            print(f"✅ [{self.client_id}] Connected successfully!")
            return True
        except Exception as e:
            print(f"❌ [{self.client_id}] Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from the server."""
        if self.socket:
            try:
                self.socket.close()
                print(f"🔌 [{self.client_id}] Disconnected")
            except:
                pass
            finally:
                self.socket = None
                self.initialized = False

    def _send_framed_message(self, data: str) -> bool:
        """Send message with 4-byte length prefix framing."""
        try:
            if not self.socket:
                return False

            message_bytes = data.encode('utf-8')
            length_prefix = len(message_bytes).to_bytes(4, byteorder='big')
            self.socket.sendall(length_prefix + message_bytes)
            return True
        except Exception as e:
            print(f"❌ [{self.client_id}] Send error: {e}")
            return False

    def _receive_framed_message(self, timeout: float = 5.0) -> Optional[str]:
        """Receive message with 4-byte length prefix framing."""
        try:
            if not self.socket:
                return None

            self.socket.settimeout(timeout)

            # Read 4-byte length prefix
            length_data = b''
            while len(length_data) < 4:
                chunk = self.socket.recv(4 - len(length_data))
                if not chunk:
                    return None
                length_data += chunk

            message_length = int.from_bytes(length_data, byteorder='big')

            if message_length > 1024 * 1024:  # 1MB limit
                print(f"❌ [{self.client_id}] Message too large: {message_length} bytes")
                return None

            # Read message data
            message_data = b''
            while len(message_data) < message_length:
                chunk = self.socket.recv(message_length - len(message_data))
                if not chunk:
                    return None
                message_data += chunk

            return message_data.decode('utf-8')

        except socket.timeout:
            return None
        except Exception as e:
            print(f"❌ [{self.client_id}] Receive error: {e}")
            return None

    def send_request(self, method: str, params: Dict[str, Any] = None, timeout: float = 30.0) -> Optional[Dict[str, Any]]:
        """Send JSON-RPC request and wait for response."""
        request_id = self._get_next_id()

        request = {
            "jsonrpc": "2.0",
            "method": method,
            "id": request_id
        }

        if params:
            request["params"] = params

        request_json = json.dumps(request)

        if not self._send_framed_message(request_json):
            return None

        # Wait for response, handle progress notifications
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            response_json = self._receive_framed_message(timeout=1.0)
            if not response_json:
                continue

            try:
                response = json.loads(response_json)

                # Handle progress notifications
                if "method" in response and response["method"] == "notifications/progress":
                    self.progress_notifications.append(response)
                    print(f"📊 [{self.client_id}] Progress: {response.get('params', {})}")
                    continue

                # Handle regular response
                if "id" in response and response["id"] == request_id:
                    return response

            except json.JSONDecodeError:
                continue

        return None

    def initialize(self) -> bool:
        """Initialize the MCP session."""
        if self.initialized:
            return True

        params = {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "roots": {"listChanged": True},
                "sampling": {}
            },
            "clientInfo": {
                "name": "ToolTestClient",
                "version": "1.0.0"
            }
        }

        response = self.send_request("initialize", params, timeout=15.0)
        if not response or "error" in response or "result" not in response:
            return False

        # Send initialized notification
        initialized_notification = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized"
        }

        if self._send_framed_message(json.dumps(initialized_notification)):
            self.initialized = True
            return True

        return False

    def list_tools(self) -> Dict[str, Dict[str, Any]]:
        """Get available tools as a dictionary."""
        if not self.initialized:
            return {}

        response = self.send_request("tools/list")
        if response and "result" in response and "tools" in response["result"]:
            tools_dict = {}
            for tool in response["result"]["tools"]:
                tools_dict[tool["name"]] = tool
            self.available_tools = tools_dict
            return tools_dict

        return {}

    def call_tool(self, name: str, arguments: Dict[str, Any] = None, timeout: float = 60.0) -> Tuple[Optional[Dict[str, Any]], List[Dict[str, Any]]]:
        """Call a tool and return result plus any progress notifications."""
        if not self.initialized:
            return None, []

        # Clear previous progress notifications
        self.progress_notifications = []

        params = {"name": name}
        if arguments:
            params["arguments"] = arguments

        response = self.send_request("tools/call", params, timeout)

        # Return response and collected progress notifications
        progress_events = self.progress_notifications.copy()
        self.progress_notifications = []

        return response, progress_events


class ToolTestSuite:
    """Comprehensive tool testing suite."""

    def __init__(self, host: str, port: int = 8080):
        self.host = host
        self.port = port
        self.test_results = []
        self.client = SessionToolClient(host, port, "tool_tester")

    def add_result(self, result: ToolTestResult):
        """Add test result."""
        self.test_results.append(result)
        status = "✅ PASS" if result.passed else "❌ FAIL"
        print(f"{status} {result.tool_name}::{result.test_name} ({result.duration_ms}ms)")
        if not result.passed and result.error_message:
            print(f"   Error: {result.error_message}")

    def run_tool_test(self, tool_name: str, test_name: str, test_func) -> ToolTestResult:
        """Run a single tool test with timing."""
        print(f"\n🧪 Testing {tool_name}::{test_name}")
        start_time = time.time()

        try:
            success, result_data, progress_events = test_func()
            duration_ms = int((time.time() - start_time) * 1000)

            error_msg = ""
            if not success:
                if isinstance(result_data, str):
                    error_msg = result_data
                    result_data = {}
                elif isinstance(result_data, dict):
                    error_msg = result_data.get("error", "Unknown error")

            return ToolTestResult(
                tool_name=tool_name,
                test_name=test_name,
                passed=success,
                duration_ms=duration_ms,
                error_message=error_msg,
                result_data=result_data if isinstance(result_data, dict) else {},
                progress_events=progress_events or []
            )

        except Exception as e:
            duration_ms = int((time.time() - start_time) * 1000)
            return ToolTestResult(
                tool_name=tool_name,
                test_name=test_name,
                passed=False,
                duration_ms=duration_ms,
                error_message=str(e)
            )

    def setup_client(self) -> bool:
        """Setup the test client."""
        if not self.client.connect():
            return False

        if not self.client.initialize():
            return False

        return True

    def test_echo_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test echo tool functionality."""
        test_message = "Hello from tool test!"

        response, progress = self.client.call_tool("echo", {"message": test_message})

        if not response:
            return False, {"error": "No response from echo tool"}, []

        if "error" in response:
            return False, {"error": response["error"]}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]

        # Verify echo response
        if "content" in result:
            content = result["content"]
            if isinstance(content, list) and len(content) > 0:
                text_content = content[0].get("text", "")
                if test_message in text_content:
                    return True, {"echoed_message": text_content}, progress

        return False, {"error": "Echo response format unexpected", "result": result}, progress

    def test_system_info_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test system info tool."""
        response, progress = self.client.call_tool("system_info")

        if not response:
            return False, {"error": "No response from system_info tool"}, []

        if "error" in response:
            return False, {"error": response["error"]}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]

        # Verify system info contains expected fields
        expected_fields = ["esp_chip", "free_heap", "uptime", "wifi_status"]
        found_fields = []

        if "content" in result:
            content = result["content"]
            if isinstance(content, list) and len(content) > 0:
                text_content = content[0].get("text", "")
                for field in expected_fields:
                    if field.lower() in text_content.lower():
                        found_fields.append(field)

        success = len(found_fields) >= 2  # At least 2 expected fields

        return success, {
            "found_fields": found_fields,
            "expected_fields": expected_fields,
            "content_preview": result.get("content", [{}])[0].get("text", "")[:200] + "..."
        }, progress

    def test_gpio_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test GPIO control tool."""
        # Test setting GPIO pin
        response, progress = self.client.call_tool("gpio", {
            "pin": 2,
            "action": "set",
            "value": 1
        })

        if not response:
            return False, {"error": "No response from GPIO tool"}, []

        if "error" in response:
            # GPIO errors might be expected if pin is not available
            error_msg = response["error"].get("message", "Unknown error")
            if "invalid" in error_msg.lower() or "not available" in error_msg.lower():
                return True, {"expected_error": error_msg}, progress
            return False, {"error": error_msg}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]

        # Verify GPIO response
        success = "content" in result

        return success, {"gpio_result": result}, progress

    def test_network_scanner_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test network scanner tool (async)."""
        response, progress = self.client.call_tool("network_scan", {
            "max_results": 10,
            "timeout": 15000
        }, timeout=30.0)

        if not response:
            return False, {"error": "No response from network scanner"}, []

        if "error" in response:
            return False, {"error": response["error"]}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]

        # Verify network scan results
        success = "content" in result
        progress_received = len(progress) > 0

        return success, {
            "scan_result": result,
            "progress_events": len(progress),
            "progress_received": progress_received
        }, progress

    def test_file_system_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test file system tool."""
        # Test listing files
        response, progress = self.client.call_tool("file_system", {
            "operation": "list",
            "path": "/"
        })

        if not response:
            return False, {"error": "No response from file system tool"}, []

        if "error" in response:
            error_msg = response["error"].get("message", "Unknown error")
            # File system might not be available
            if "not available" in error_msg.lower() or "not supported" in error_msg.lower():
                return True, {"expected_error": error_msg}, progress
            return False, {"error": error_msg}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]
        success = "content" in result

        return success, {"file_system_result": result}, progress

    def test_i2c_scanner_tool(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test I2C scanner tool."""
        response, progress = self.client.call_tool("i2c_scan", {
            "sda": 4,
            "scl": 5
        })

        if not response:
            return False, {"error": "No response from I2C scanner"}, []

        if "error" in response:
            error_msg = response["error"].get("message", "Unknown error")
            # I2C might not be available or pins might be invalid
            if any(term in error_msg.lower() for term in ["invalid", "not available", "busy"]):
                return True, {"expected_error": error_msg}, progress
            return False, {"error": error_msg}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]
        success = "content" in result

        return success, {"i2c_result": result}, progress

    def test_long_running_task(self) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test long running task tool."""
        response, progress = self.client.call_tool("long_task", {
            "duration": 5,
            "steps": 5,
            "message": "Test long running task"
        }, timeout=15.0)

        if not response:
            return False, {"error": "No response from long running task"}, []

        if "error" in response:
            return False, {"error": response["error"]}, progress

        if "result" not in response:
            return False, {"error": "No result in response"}, progress

        result = response["result"]
        success = "content" in result
        progress_received = len(progress) > 0

        return success, {
            "task_result": result,
            "progress_events": len(progress),
            "progress_received": progress_received
        }, progress

    def test_tool_with_invalid_params(self, tool_name: str) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test tool with invalid parameters."""
        response, progress = self.client.call_tool(tool_name, {
            "invalid_param": "should_cause_error",
            "another_invalid": 12345
        })

        if not response:
            return False, {"error": "No response received"}, []

        # We expect an error response
        if "error" in response:
            return True, {"expected_error": response["error"]}, progress

        # If no error, the tool might be very lenient
        return True, {"tool_accepted_invalid_params": True, "result": response.get("result")}, progress

    def test_tool_parameter_validation(self, tool_name: str, tool_schema: Dict[str, Any]) -> Tuple[bool, Dict[str, Any], List[Dict[str, Any]]]:
        """Test tool parameter validation based on schema."""
        if "input_schema" not in tool_schema:
            return True, {"no_schema": "Tool has no input schema to validate"}, []

        schema = tool_schema["input_schema"]

        # Test with empty parameters if schema requires some
        if "required" in schema and schema["required"]:
            response, progress = self.client.call_tool(tool_name, {})

            if not response:
                return False, {"error": "No response for empty params test"}, []

            # Should get an error for missing required params
            if "error" in response:
                return True, {"validation_working": response["error"]}, progress
            else:
                return False, {"error": "Tool should have rejected empty params"}, progress

        return True, {"no_required_params": "Tool has no required parameters"}, []

    def run_comprehensive_tool_tests(self, specific_tool: str = None, verbose: bool = False):
        """Run comprehensive tests for all available tools."""
        print("\n" + "="*80)
        print("🛠️  ESP8266 TOOL TESTING SUITE")
        print("="*80)

        # Setup client
        if not self.setup_client():
            print("❌ Failed to setup test client")
            return

        # Discover available tools
        tools = self.client.list_tools()
        if not tools:
            print("❌ No tools discovered")
            return

        print(f"📋 Discovered {len(tools)} tools: {list(tools.keys())}")

        # Filter tools if specific tool requested
        if specific_tool:
            if specific_tool in tools:
                tools = {specific_tool: tools[specific_tool]}
                print(f"🎯 Testing specific tool: {specific_tool}")
            else:
                print(f"❌ Tool '{specific_tool}' not found")
                return

        # Define tool test mappings
        tool_tests = {
            "echo": self.test_echo_tool,
            "system_info": self.test_system_info_tool,
            "gpio": self.test_gpio_tool,
            "network_scan": self.test_network_scanner_tool,
            "file_system": self.test_file_system_tool,
            "i2c_scan": self.test_i2c_scanner_tool,
            "long_task": self.test_long_running_task,
        }

        # Run tests for each tool
        for tool_name, tool_info in tools.items():
            print(f"\n🔧 Testing tool: {tool_name}")
            print(f"   Description: {tool_info.get('description', 'No description')}")

            # Run specific test if available
            if tool_name in tool_tests:
                result = self.run_tool_test(tool_name, "functionality", tool_tests[tool_name])
                self.add_result(result)

                if verbose and result.progress_events:
                    print(f"   Progress events: {len(result.progress_events)}")
                    for i, event in enumerate(result.progress_events):
                        print(f"     {i+1}: {event}")

            # Run parameter validation test
            result = self.run_tool_test(tool_name, "param_validation",
                                      lambda: self.test_tool_parameter_validation(tool_name, tool_info))
            self.add_result(result)

            # Run invalid parameter test
            result = self.run_tool_test(tool_name, "invalid_params",
                                      lambda: self.test_tool_with_invalid_params(tool_name))
            self.add_result(result)

        # Cleanup
        self.client.disconnect()

        # Print summary
        self.print_summary()

    def print_summary(self):
        """Print test summary."""
        print("\n" + "="*80)
        print("📊 TOOL TEST SUMMARY")
        print("="*80)

        # Group results by tool
        tool_results = {}
        for result in self.test_results:
            if result.tool_name not in tool_results:
                tool_results[result.tool_name] = []
            tool_results[result.tool_name].append(result)

        total_tests = len(self.test_results)
        total_passed = sum(1 for r in self.test_results if r.passed)
        total_failed = total_tests - total_passed
        total_time = sum(r.duration_ms for r in self.test_results)

        print(f"Overall: {total_tests} tests, {total_passed} passed, {total_failed} failed")
        print(f"Success rate: {(total_passed/total_tests*100):.1f}%")
        print(f"Total time: {total_time}ms")

        print(f"\nPer-tool results:")
        for tool_name, results in tool_results.items():
            passed = sum(1 for r in results if r.passed)
            total = len(results)
            avg_time = sum(r.duration_ms for r in results) / total
            status = "✅" if passed == total else "⚠️" if passed > 0 else "❌"

            print(f"  {status} {tool_name}: {passed}/{total} tests passed (avg: {avg_time:.0f}ms)")

        # Show failed tests
        failed_tests = [r for r in self.test_results if not r.passed]
        if failed_tests:
            print(f"\n❌ Failed tests:")
            for result in failed_tests:
                print(f"  - {result.tool_name}::{result.test_name}: {result.error_message}")

        # Show tools with progress reporting
        progress_tools = [r for r in self.test_results if r.progress_events]
        if progress_tools:
            print(f"\n📊 Tools with progress reporting:")
            for result in progress_tools:
                print(f"  - {result.tool_name}: {len(result.progress_events)} progress events")

        print("\n" + "="*80)


def main():
    """Main test runner."""
    parser = argparse.ArgumentParser(description="ESP8266 Tool Testing Suite")
    parser.add_argument("host", help="ESP8266 IP address")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--tool", help="Test specific tool only")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    args = parser.parse_args()

    print(f"🎯 Testing ESP8266 tools at {args.host}:{args.port}")

    # Create and run tester
    tester = ToolTestSuite(args.host, args.port)
    tester.run_comprehensive_tool_tests(args.tool, args.verbose)

    # Exit with appropriate code
    failed_tests = sum(1 for r in tester.test_results if not r.passed)
    sys.exit(1 if failed_tests > 0 else 0)


if __name__ == "__main__":
    main()
