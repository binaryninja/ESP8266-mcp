#!/usr/bin/env python3
"""
ESP8266 MCP Client Test Script

This script provides a comprehensive test client for the ESP8266 MCP server.
It tests all MCP protocol features including initialization, tool discovery,
and tool execution.

Usage:
    python test_mcp_client.py <ESP8266_IP_ADDRESS> [port]

Example:
    python test_mcp_client.py 192.168.1.100
    python test_mcp_client.py 192.168.1.100 8080
"""

import json
import socket
import sys
import time
import argparse
from typing import Dict, Any, Optional, List


class MCPClient:
    """MCP (Machine Control Protocol) client for testing ESP8266 MCP server."""

    def __init__(self, host: str, port: int = 8080):
        self.host = host
        self.port = port
        self.socket = None
        self.request_id = 0
        self.initialized = False
        self.server_info = {}
        self.available_tools = []

    def connect(self) -> bool:
        """Connect to the MCP server."""
        try:
            print(f"🔗 Connecting to MCP server at {self.host}:{self.port}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)  # 10 second timeout
            self.socket.connect((self.host, self.port))
            print("✅ Connected successfully!")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from the MCP server."""
        if self.socket:
            try:
                self.socket.close()
                print("🔌 Disconnected from server")
            except:
                pass
            self.socket = None

    def send_request(self, method: str, params: Optional[Dict[str, Any]] = None) -> Optional[Dict[str, Any]]:
        """Send a JSON-RPC request to the server."""
        if not self.socket:
            print("❌ Not connected to server")
            return None

        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": str(self.request_id),
            "method": method
        }

        if params:
            request["params"] = params

        try:
            # Send request
            request_json = json.dumps(request)
            print(f"📤 Sending: {request_json}")
            bytes_sent = self.socket.send((request_json + "\n").encode('utf-8'))
            print(f"📤 Sent {bytes_sent} bytes successfully")

            # Receive response
            response_data = ""
            response_line = ""
            print("📥 Waiting for response...")
            start_time = time.time()
            while True:
                try:
                    chunk = self.socket.recv(4096).decode('utf-8')
                    if not chunk:
                        print("❌ Connection closed by server")
                        break
                    response_data += chunk
                    print(f"📥 Received chunk: {repr(chunk)}")
                    # Look for complete JSON response (newline terminated)
                    if '\n' in response_data:
                        response_line = response_data.split('\n')[0]
                        print(f"📥 Complete response received: {response_line}")
                        break
                except socket.timeout:
                    elapsed = time.time() - start_time
                    print(f"⏱️  Socket timeout after {elapsed:.1f}s")
                    break

            if not response_line:
                print("❌ No response received")
                return None

            print(f"📥 Parsing response: {response_line}")
            response = json.loads(response_line)

            # Don't filter out error responses here - let the caller handle them
            return response

        except json.JSONDecodeError as e:
            print(f"❌ JSON decode error: {e}")
            print(f"Raw response: {response_data}")
            return None
        except Exception as e:
            print(f"❌ Request failed: {e}")
            return None

    def initialize(self) -> bool:
        """Initialize the MCP session."""
        print("\n🚀 Initializing MCP session...")

        params = {
            "protocolVersion": "2024-11-05",
            "clientInfo": {
                "name": "ESP8266-MCP-Test-Client",
                "version": "1.0.0"
            },
            "capabilities": {
                "roots": {
                    "listChanged": False
                },
                "sampling": {}
            }
        }

        response = self.send_request("initialize", params)
        if not response:
            return False

        if "result" in response:
            result = response["result"]
            self.server_info = result.get("serverInfo", {})
            capabilities = result.get("capabilities", {})

            print(f"✅ Server initialized!")
            print(f"   📋 Server: {self.server_info.get('name', 'Unknown')} v{self.server_info.get('version', 'Unknown')}")
            print(f"   🔧 Protocol: {result.get('protocolVersion', 'Unknown')}")
            print(f"   🛠️  Tools supported: {capabilities.get('tools', {}).get('listChanged', 'Unknown')}")

            self.initialized = True
            return True

        return False

    def ping(self) -> bool:
        """Send a ping request to test connectivity."""
        print("\n🏓 Sending ping...")
        response = self.send_request("ping")
        if response and "result" in response:
            print("✅ Ping successful!")
            return True
        print("❌ Ping failed!")
        return False

    def list_tools(self) -> bool:
        """List available tools on the server."""
        print("\n🔍 Discovering available tools...")

        if not self.initialized:
            print("❌ Session not initialized. Call initialize() first.")
            return False

        response = self.send_request("tools/list")
        if not response or "result" not in response:
            return False

        result = response["result"]
        self.available_tools = result.get("tools", [])

        print(f"✅ Found {len(self.available_tools)} tools:")
        for i, tool in enumerate(self.available_tools, 1):
            name = tool.get("name", "Unknown")
            description = tool.get("description", "No description")
            print(f"   {i}. 🛠️  {name}: {description}")

            # Show input schema if available
            schema = tool.get("inputSchema", {})
            if schema.get("properties"):
                print(f"      📝 Parameters:")
                for param_name, param_info in schema["properties"].items():
                    param_type = param_info.get("type", "unknown")
                    param_desc = param_info.get("description", "")
                    required = param_name in schema.get("required", [])
                    req_marker = " (required)" if required else " (optional)"
                    print(f"         • {param_name} ({param_type}){req_marker}: {param_desc}")

        return True

    def call_tool(self, tool_name: str, arguments: Dict[str, Any]) -> bool:
        """Call a specific tool with given arguments."""
        print(f"\n🔧 Calling tool '{tool_name}' with arguments: {arguments}")

        if not self.initialized:
            print("❌ Session not initialized. Call initialize() first.")
            return False

        params = {
            "name": tool_name,
            "arguments": arguments
        }

        response = self.send_request("tools/call", params)
        if not response:
            return False

        # Check for error response
        if "error" in response:
            error = response["error"]
            print(f"❌ Tool error {error['code']}: {error['message']}")
            return False

        # Check for success response
        if "result" not in response:
            print("❌ Invalid response: no result or error field")
            return False

        result = response["result"]
        content = result.get("content", [])

        print(f"✅ Tool '{tool_name}' executed successfully!")
        print("📄 Response content:")
        for item in content:
            content_type = item.get("type", "unknown")
            if content_type == "text":
                text = item.get("text", "")
                print(f"   📝 {text}")
            else:
                print(f"   🔍 {content_type}: {item}")

        return True

    def test_echo_tool(self) -> bool:
        """Test the echo tool."""
        test_messages = [
            "Hello ESP8266!",
            "Testing MCP protocol 🚀",
            "Special characters: !@#$%^&*()",
            "Multiple\nlines\ntest"
        ]

        success_count = 0
        for i, message in enumerate(test_messages, 1):
            print(f"\n🔊 Echo Test {i}/{len(test_messages)}")
            if self.call_tool("echo", {"text": message}):
                success_count += 1
            time.sleep(0.5)  # Small delay between tests

        print(f"\n📊 Echo test results: {success_count}/{len(test_messages)} passed")
        return success_count == len(test_messages)

    def test_gpio_tool(self) -> bool:
        """Test the GPIO control tool."""
        gpio_tests = [
            {"pin": 2, "state": "high"},   # Built-in LED (usually pin 2)
            {"pin": 2, "state": "low"},
            {"pin": 16, "state": "high"},  # Common GPIO pin
            {"pin": 16, "state": "low"},
            {"pin": 0, "state": "high"},   # GPIO 0
            {"pin": 0, "state": "low"}
        ]

        success_count = 0
        for i, test in enumerate(gpio_tests, 1):
            print(f"\n🔌 GPIO Test {i}/{len(gpio_tests)}")
            if self.call_tool("gpio_control", test):
                success_count += 1
            time.sleep(1)  # Delay to see LED changes if connected

        print(f"\n📊 GPIO test results: {success_count}/{len(gpio_tests)} passed")
        return success_count == len(gpio_tests)

    def run_comprehensive_test(self) -> bool:
        """Run a comprehensive test of all MCP features."""
        print("🧪 Starting comprehensive MCP test suite...")
        print("=" * 60)

        test_results = []

        # Test 1: Connection
        print("\n📡 TEST 1: Connection")
        if self.connect():
            test_results.append(("Connection", True))
        else:
            test_results.append(("Connection", False))
            return False

        # Test 2: Initialization
        print("\n🚀 TEST 2: Initialization")
        init_result = self.initialize()
        test_results.append(("Initialization", init_result))

        if not init_result:
            return False

        # Test 3: Ping
        print("\n🏓 TEST 3: Ping")
        ping_result = self.ping()
        test_results.append(("Ping", ping_result))

        # Test 4: Tool Discovery
        print("\n🔍 TEST 4: Tool Discovery")
        discovery_result = self.list_tools()
        test_results.append(("Tool Discovery", discovery_result))

        # Test 5: Echo Tool
        print("\n🔊 TEST 5: Echo Tool")
        echo_result = self.test_echo_tool()
        test_results.append(("Echo Tool", echo_result))

        # Test 6: GPIO Tool
        print("\n🔌 TEST 6: GPIO Tool")
        gpio_result = self.test_gpio_tool()
        test_results.append(("GPIO Tool", gpio_result))

        # Test 7: Error Handling (invalid tool)
        print("\n❌ TEST 7: Error Handling")
        print("Testing invalid tool call...")
        error_test_result = self.test_error_handling("nonexistent_tool", {})
        test_results.append(("Error Handling", error_test_result))

        # Test 8: Invalid parameters
        print("\n❌ TEST 8: Invalid Parameters")
        print("Testing invalid parameters...")
        invalid_param_result = self.test_error_handling("echo", {"wrong_param": "test"})
        test_results.append(("Invalid Parameters", invalid_param_result))

        # Print final results
        print("\n" + "=" * 60)
        print("🏁 COMPREHENSIVE TEST RESULTS")
        print("=" * 60)

        passed = 0
        total = len(test_results)

        for test_name, result in test_results:
            status = "✅ PASS" if result else "❌ FAIL"
            print(f"{status} - {test_name}")
            if result:
                passed += 1

        print(f"\n📊 Overall Results: {passed}/{total} tests passed")
        print(f"🎯 Success Rate: {(passed/total)*100:.1f}%")

        if passed == total:
            print("🎉 All tests passed! ESP8266 MCP server is working perfectly!")
        else:
            print("⚠️  Some tests failed. Check the server implementation.")

        return passed == total

    def test_error_handling(self, tool_name: str, arguments: Dict[str, Any]) -> bool:
        """Test that a tool call properly returns an error response."""
        print(f"\n🔧 Testing error case: tool '{tool_name}' with arguments: {arguments}")

        if not self.initialized:
            print("❌ Session not initialized. Call initialize() first.")
            return False

        params = {
            "name": tool_name,
            "arguments": arguments
        }

        response = self.send_request("tools/call", params)
        if not response:
            print("❌ No response received (timeout)")
            return False

        # For error testing, we expect an error response
        if "error" in response:
            error = response["error"]
            print(f"✅ Received expected error {error['code']}: {error['message']}")
            return True
        elif "result" in response:
            print("❌ Unexpected success response - should have been an error")
            return False
        else:
            print("❌ Invalid response: no result or error field")
            return False


def main():
    """Main function to run the MCP client test."""
    parser = argparse.ArgumentParser(
        description="ESP8266 MCP Client Test Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python test_mcp_client.py 192.168.1.100
  python test_mcp_client.py 192.168.1.100 8080
  python test_mcp_client.py 192.168.4.1 --interactive
        """
    )

    parser.add_argument("host", help="ESP8266 IP address")
    parser.add_argument("port", type=int, nargs="?", default=8080, help="Port number (default: 8080)")
    parser.add_argument("--interactive", "-i", action="store_true", help="Run in interactive mode")
    parser.add_argument("--ping-only", action="store_true", help="Only test ping connectivity")

    args = parser.parse_args()

    print("🤖 ESP8266 MCP Client Test Script")
    print("=" * 60)
    print(f"Target: {args.host}:{args.port}")
    print("=" * 60)

    client = MCPClient(args.host, args.port)

    try:
        if args.ping_only:
            # Simple connectivity test
            if client.connect():
                if client.initialize():
                    client.ping()
                client.disconnect()
        elif args.interactive:
            # Interactive mode
            if not client.connect():
                return 1

            if not client.initialize():
                client.disconnect()
                return 1

            print("\n🎮 Interactive Mode - Available commands:")
            print("  ping                 - Test connectivity")
            print("  list                 - List available tools")
            print("  echo <message>       - Test echo tool")
            print("  gpio <pin> <state>   - Control GPIO (state: high/low)")
            print("  test                 - Run comprehensive test")
            print("  quit                 - Exit")

            while True:
                try:
                    command = input("\n> ").strip().split()
                    if not command:
                        continue

                    cmd = command[0].lower()

                    if cmd == "quit":
                        break
                    elif cmd == "ping":
                        client.ping()
                    elif cmd == "list":
                        client.list_tools()
                    elif cmd == "echo" and len(command) > 1:
                        message = " ".join(command[1:])
                        client.call_tool("echo", {"text": message})
                    elif cmd == "gpio" and len(command) >= 3:
                        try:
                            pin = int(command[1])
                            state = command[2].lower()
                            if state in ["high", "low"]:
                                client.call_tool("gpio_control", {"pin": pin, "state": state})
                            else:
                                print("❌ State must be 'high' or 'low'")
                        except ValueError:
                            print("❌ Pin must be a number")
                    elif cmd == "test":
                        client.run_comprehensive_test()
                    else:
                        print("❌ Unknown command or invalid arguments")

                except KeyboardInterrupt:
                    print("\n🛑 Interrupted by user")
                    break
                except EOFError:
                    break

            client.disconnect()
        else:
            # Run comprehensive test by default
            success = client.run_comprehensive_test()
            client.disconnect()
            return 0 if success else 1

    except KeyboardInterrupt:
        print("\n🛑 Test interrupted by user")
        client.disconnect()
        return 1
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        client.disconnect()
        return 1


if __name__ == "__main__":
    sys.exit(main())
