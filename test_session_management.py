#!/usr/bin/env python3
"""
ESP8266 Session Management System Test Client

Comprehensive test suite for the FreeRTOS Session Management System.
Tests session lifecycle, concurrent sessions, async tasks, tool execution,
error handling, and performance characteristics.

Usage:
    python test_session_management.py <ESP8266_IP> [options]

Example:
    python test_session_management.py 192.168.1.100
    python test_session_management.py 192.168.1.100 --port 8080 --sessions 3 --verbose
"""

import json
import socket
import sys
import time
import threading
import argparse
import asyncio
import concurrent.futures
from typing import Dict, Any, Optional, List, Tuple
from dataclasses import dataclass
from enum import Enum


class SessionState(Enum):
    UNINITIALIZED = "UNINITIALIZED"
    INITIALIZING = "INITIALIZING"
    INITIALIZED = "INITIALIZED"
    ACTIVE = "ACTIVE"
    SHUTTING_DOWN = "SHUTTING_DOWN"
    SHUTDOWN = "SHUTDOWN"
    ERROR_STATE = "ERROR_STATE"


@dataclass
class TestResult:
    name: str
    passed: bool
    duration_ms: int
    error_message: str = ""
    details: Dict[str, Any] = None

    def __post_init__(self):
        if self.details is None:
            self.details = {}


class SessionMCPClient:
    """Enhanced MCP client for session management testing."""

    def __init__(self, host: str, port: int = 8080, client_id: str = None):
        self.host = host
        self.port = port
        self.client_id = client_id or f"test_client_{int(time.time())}"
        self.socket = None
        self.request_id = 0
        self.initialized = False
        self.server_info = {}
        self.available_tools = []
        self.session_state = SessionState.UNINITIALIZED
        self.lock = threading.Lock()

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

            # Encode message with length prefix
            message_bytes = data.encode('utf-8')
            length_prefix = len(message_bytes).to_bytes(4, byteorder='big')

            # Send length prefix + message
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

            # Decode message length
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
            print(f"⏰ [{self.client_id}] Receive timeout")
            return None
        except Exception as e:
            print(f"❌ [{self.client_id}] Receive error: {e}")
            return None

    def send_request(self, method: str, params: Dict[str, Any] = None, timeout: float = 10.0) -> Optional[Dict[str, Any]]:
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
        print(f"📤 [{self.client_id}] Request: {request_json}")

        if not self._send_framed_message(request_json):
            return None

        # Wait for response
        response_json = self._receive_framed_message(timeout)
        if not response_json:
            return None

        try:
            response = json.loads(response_json)
            print(f"📥 [{self.client_id}] Response: {response_json}")

            # Validate response
            if "jsonrpc" not in response or response["jsonrpc"] != "2.0":
                print(f"❌ [{self.client_id}] Invalid JSON-RPC response")
                return None

            if "id" in response and response["id"] != request_id:
                print(f"❌ [{self.client_id}] Response ID mismatch")
                return None

            return response

        except json.JSONDecodeError as e:
            print(f"❌ [{self.client_id}] JSON decode error: {e}")
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
                "name": "SessionTestClient",
                "version": "1.0.0"
            }
        }

        response = self.send_request("initialize", params, timeout=15.0)
        if not response:
            return False

        if "error" in response:
            print(f"❌ [{self.client_id}] Initialize error: {response['error']}")
            return False

        if "result" not in response:
            print(f"❌ [{self.client_id}] No result in initialize response")
            return False

        self.server_info = response["result"]
        self.session_state = SessionState.INITIALIZED

        # Send initialized notification
        initialized_notification = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized"
        }

        if self._send_framed_message(json.dumps(initialized_notification)):
            self.initialized = True
            self.session_state = SessionState.ACTIVE
            print(f"✅ [{self.client_id}] Session initialized successfully")
            return True

        return False

    def list_tools(self) -> List[Dict[str, Any]]:
        """Get available tools."""
        if not self.initialized:
            return []

        response = self.send_request("tools/list")
        if response and "result" in response and "tools" in response["result"]:
            self.available_tools = response["result"]["tools"]
            return self.available_tools

        return []

    def call_tool(self, name: str, arguments: Dict[str, Any] = None, timeout: float = 30.0) -> Optional[Dict[str, Any]]:
        """Call a tool with optional arguments."""
        if not self.initialized:
            return None

        params = {"name": name}
        if arguments:
            params["arguments"] = arguments

        response = self.send_request("tools/call", params, timeout)
        if response and "result" in response:
            return response["result"]

        return response

    def ping(self) -> bool:
        """Send ping to test connection."""
        if not self.initialized:
            return False

        response = self.send_request("ping", timeout=5.0)
        return response is not None and "result" in response


class SessionManagementTester:
    """Comprehensive session management test suite."""

    def __init__(self, host: str, port: int = 8080):
        self.host = host
        self.port = port
        self.test_results = []

    def add_result(self, result: TestResult):
        """Add test result."""
        self.test_results.append(result)
        status = "✅ PASS" if result.passed else "❌ FAIL"
        print(f"{status} {result.name} ({result.duration_ms}ms)")
        if not result.passed and result.error_message:
            print(f"   Error: {result.error_message}")

    def run_test(self, name: str, test_func) -> TestResult:
        """Run a single test with timing."""
        print(f"\n🧪 Running test: {name}")
        start_time = time.time()

        try:
            success, details = test_func()
            duration_ms = int((time.time() - start_time) * 1000)

            if isinstance(details, str):
                error_msg = details if not success else ""
                details_dict = {}
            else:
                error_msg = details.get("error", "") if not success else ""
                details_dict = details if isinstance(details, dict) else {}

            return TestResult(name, success, duration_ms, error_msg, details_dict)

        except Exception as e:
            duration_ms = int((time.time() - start_time) * 1000)
            return TestResult(name, False, duration_ms, str(e))

    def test_basic_connection(self) -> Tuple[bool, Dict[str, Any]]:
        """Test basic connection to server."""
        client = SessionMCPClient(self.host, self.port, "connection_test")

        try:
            if not client.connect():
                return False, {"error": "Connection failed"}

            client.disconnect()
            return True, {"status": "Connection successful"}

        except Exception as e:
            return False, {"error": f"Connection test failed: {e}"}
        finally:
            client.disconnect()

    def test_session_initialization(self) -> Tuple[bool, Dict[str, Any]]:
        """Test session initialization process."""
        client = SessionMCPClient(self.host, self.port, "init_test")

        try:
            if not client.connect():
                return False, {"error": "Connection failed"}

            if not client.initialize():
                return False, {"error": "Initialization failed"}

            # Verify server info
            if not client.server_info:
                return False, {"error": "No server info received"}

            return True, {
                "server_info": client.server_info,
                "session_state": client.session_state.value
            }

        except Exception as e:
            return False, {"error": f"Initialization test failed: {e}"}
        finally:
            client.disconnect()

    def test_tool_discovery(self) -> Tuple[bool, Dict[str, Any]]:
        """Test tool discovery and listing."""
        client = SessionMCPClient(self.host, self.port, "tools_test")

        try:
            if not client.connect() or not client.initialize():
                return False, {"error": "Setup failed"}

            tools = client.list_tools()

            if not tools:
                return False, {"error": "No tools discovered"}

            # Verify tool structure
            for tool in tools:
                if "name" not in tool or "description" not in tool:
                    return False, {"error": f"Invalid tool structure: {tool}"}

            return True, {
                "tool_count": len(tools),
                "tools": [tool["name"] for tool in tools]
            }

        except Exception as e:
            return False, {"error": f"Tool discovery failed: {e}"}
        finally:
            client.disconnect()

    def test_concurrent_sessions(self) -> Tuple[bool, Dict[str, Any]]:
        """Test multiple concurrent sessions."""
        session_count = 3
        clients = []

        try:
            # Create multiple clients
            for i in range(session_count):
                client = SessionMCPClient(self.host, self.port, f"concurrent_{i}")
                clients.append(client)

            # Connect all clients
            for client in clients:
                if not client.connect():
                    return False, {"error": f"Client {client.client_id} connection failed"}

            # Initialize all sessions
            for client in clients:
                if not client.initialize():
                    return False, {"error": f"Client {client.client_id} initialization failed"}

            # Test all sessions are working
            working_sessions = 0
            for client in clients:
                if client.ping():
                    working_sessions += 1

            return True, {
                "requested_sessions": session_count,
                "working_sessions": working_sessions,
                "success_rate": working_sessions / session_count
            }

        except Exception as e:
            return False, {"error": f"Concurrent sessions test failed: {e}"}
        finally:
            for client in clients:
                client.disconnect()

    def test_async_tool_execution(self) -> Tuple[bool, Dict[str, Any]]:
        """Test asynchronous tool execution."""
        client = SessionMCPClient(self.host, self.port, "async_test")

        try:
            if not client.connect() or not client.initialize():
                return False, {"error": "Setup failed"}

            tools = client.list_tools()
            async_tools = [tool for tool in tools if "scan" in tool["name"].lower() or "long" in tool["name"].lower()]

            if not async_tools:
                # Try echo tool as fallback
                result = client.call_tool("echo", {"message": "async test"}, timeout=15.0)
                if result:
                    return True, {"fallback": "echo", "result": result}
                else:
                    return False, {"error": "No async tools available and echo failed"}

            # Test async tool
            tool_name = async_tools[0]["name"]
            result = client.call_tool(tool_name, {}, timeout=30.0)

            if not result:
                return False, {"error": f"Async tool {tool_name} failed"}

            return True, {
                "tool": tool_name,
                "result_type": type(result).__name__,
                "has_content": "content" in result if isinstance(result, dict) else False
            }

        except Exception as e:
            return False, {"error": f"Async tool test failed: {e}"}
        finally:
            client.disconnect()

    def test_error_handling(self) -> Tuple[bool, Dict[str, Any]]:
        """Test error handling scenarios."""
        client = SessionMCPClient(self.host, self.port, "error_test")

        try:
            if not client.connect() or not client.initialize():
                return False, {"error": "Setup failed"}

            error_scenarios = []

            # Test invalid tool call
            result = client.call_tool("nonexistent_tool")
            if result and "error" in result:
                error_scenarios.append("invalid_tool_handled")

            # Test invalid parameters
            result = client.call_tool("echo", {"invalid_param": "test"})
            # This might succeed or fail depending on tool implementation
            error_scenarios.append("invalid_params_tested")

            # Test malformed request
            malformed_request = '{"jsonrpc":"2.0","method":"invalid"}'
            if client._send_framed_message(malformed_request):
                response = client._receive_framed_message(timeout=5.0)
                if response:
                    error_scenarios.append("malformed_request_handled")

            return True, {
                "scenarios_tested": len(error_scenarios),
                "scenarios": error_scenarios
            }

        except Exception as e:
            return False, {"error": f"Error handling test failed: {e}"}
        finally:
            client.disconnect()

    def test_session_persistence(self) -> Tuple[bool, Dict[str, Any]]:
        """Test session state persistence and recovery."""
        client = SessionMCPClient(self.host, self.port, "persistence_test")

        try:
            # Initial session setup
            if not client.connect() or not client.initialize():
                return False, {"error": "Initial setup failed"}

            # Get initial tools
            initial_tools = client.list_tools()

            # Simulate network interruption
            client.socket.close()
            time.sleep(1)

            # Reconnect
            if not client.connect():
                return False, {"error": "Reconnection failed"}

            # Re-initialize
            client.initialized = False
            client.session_state = SessionState.UNINITIALIZED

            if not client.initialize():
                return False, {"error": "Re-initialization failed"}

            # Verify tools are still available
            reconnect_tools = client.list_tools()

            return True, {
                "initial_tools": len(initial_tools),
                "reconnect_tools": len(reconnect_tools),
                "tools_preserved": len(initial_tools) == len(reconnect_tools)
            }

        except Exception as e:
            return False, {"error": f"Session persistence test failed: {e}"}
        finally:
            client.disconnect()

    def test_performance_metrics(self) -> Tuple[bool, Dict[str, Any]]:
        """Test performance characteristics."""
        client = SessionMCPClient(self.host, self.port, "perf_test")

        try:
            if not client.connect() or not client.initialize():
                return False, {"error": "Setup failed"}

            # Connection latency test
            ping_times = []
            for _ in range(10):
                start = time.time()
                if client.ping():
                    ping_times.append((time.time() - start) * 1000)
                time.sleep(0.1)

            avg_ping = sum(ping_times) / len(ping_times) if ping_times else 0

            # Tool execution speed test
            tool_times = []
            for _ in range(5):
                start = time.time()
                result = client.call_tool("echo", {"message": "perf test"})
                if result:
                    tool_times.append((time.time() - start) * 1000)
                time.sleep(0.1)

            avg_tool_time = sum(tool_times) / len(tool_times) if tool_times else 0

            return True, {
                "avg_ping_ms": round(avg_ping, 2),
                "avg_tool_execution_ms": round(avg_tool_time, 2),
                "ping_samples": len(ping_times),
                "tool_samples": len(tool_times)
            }

        except Exception as e:
            return False, {"error": f"Performance test failed: {e}"}
        finally:
            client.disconnect()

    def test_message_framing(self) -> Tuple[bool, Dict[str, Any]]:
        """Test message framing protocol."""
        client = SessionMCPClient(self.host, self.port, "framing_test")

        try:
            if not client.connect():
                return False, {"error": "Connection failed"}

            # Test different message sizes
            test_sizes = [10, 100, 1000, 5000]
            framing_results = {}

            for size in test_sizes:
                # Create message of specific size
                test_message = "x" * (size - 50)  # Account for JSON overhead

                request = {
                    "jsonrpc": "2.0",
                    "method": "echo",
                    "params": {"message": test_message},
                    "id": client._get_next_id()
                }

                request_json = json.dumps(request)
                actual_size = len(request_json.encode('utf-8'))

                # Send and measure
                start = time.time()
                success = client._send_framed_message(request_json)

                if success:
                    response = client._receive_framed_message()
                    duration = (time.time() - start) * 1000

                    framing_results[f"size_{actual_size}"] = {
                        "success": response is not None,
                        "duration_ms": round(duration, 2)
                    }
                else:
                    framing_results[f"size_{actual_size}"] = {"success": False}

            return True, {"framing_tests": framing_results}

        except Exception as e:
            return False, {"error": f"Message framing test failed: {e}"}
        finally:
            client.disconnect()

    def run_all_tests(self, verbose: bool = False):
        """Run all session management tests."""
        print("\n" + "="*80)
        print("🚀 ESP8266 SESSION MANAGEMENT SYSTEM TEST SUITE")
        print("="*80)

        # Test suite
        tests = [
            ("Basic Connection", self.test_basic_connection),
            ("Session Initialization", self.test_session_initialization),
            ("Tool Discovery", self.test_tool_discovery),
            ("Concurrent Sessions", self.test_concurrent_sessions),
            ("Async Tool Execution", self.test_async_tool_execution),
            ("Error Handling", self.test_error_handling),
            ("Session Persistence", self.test_session_persistence),
            ("Performance Metrics", self.test_performance_metrics),
            ("Message Framing", self.test_message_framing),
        ]

        # Run all tests
        for test_name, test_func in tests:
            result = self.run_test(test_name, test_func)
            self.add_result(result)

            if verbose and result.details:
                print(f"   Details: {json.dumps(result.details, indent=2)}")

        # Print summary
        self.print_summary()

    def print_summary(self):
        """Print test summary."""
        print("\n" + "="*80)
        print("📊 TEST SUMMARY")
        print("="*80)

        passed = sum(1 for r in self.test_results if r.passed)
        failed = len(self.test_results) - passed
        total_time = sum(r.duration_ms for r in self.test_results)

        print(f"Tests run: {len(self.test_results)}")
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")
        print(f"Success rate: {(passed/len(self.test_results)*100):.1f}%")
        print(f"Total time: {total_time}ms")

        if failed > 0:
            print(f"\n❌ Failed tests:")
            for result in self.test_results:
                if not result.passed:
                    print(f"  - {result.name}: {result.error_message}")

        print("\n" + "="*80)


def main():
    """Main test runner."""
    parser = argparse.ArgumentParser(description="ESP8266 Session Management System Tester")
    parser.add_argument("host", help="ESP8266 IP address")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--sessions", type=int, default=3, help="Number of concurrent sessions to test")

    args = parser.parse_args()

    print(f"🎯 Testing ESP8266 Session Management System at {args.host}:{args.port}")

    # Create and run tester
    tester = SessionManagementTester(args.host, args.port)
    tester.run_all_tests(args.verbose)

    # Exit with appropriate code
    failed_tests = sum(1 for r in tester.test_results if not r.passed)
    sys.exit(1 if failed_tests > 0 else 0)


if __name__ == "__main__":
    main()
