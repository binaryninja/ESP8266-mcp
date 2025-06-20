#!/usr/bin/env python3
"""
Enhanced JSON serialization debugging test for ESP8266 MCP Server
Focuses on identifying where string values are being serialized as boolean 'true'
"""

import socket
import json
import time
import sys
from typing import Optional, Dict, Any, List

class JSONSerializationDebugger:
    def __init__(self, host: str, port: int = 8080):
        self.host = host
        self.port = port
        self.socket = None
        self.request_id = 0

    def connect(self) -> bool:
        """Connect to the ESP8266 MCP server."""
        try:
            print(f"🔗 Connecting to {self.host}:{self.port}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((self.host, self.port))
            print("✅ Connected successfully")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from the server."""
        if self.socket:
            self.socket.close()
            self.socket = None
            print("🔌 Disconnected")

    def send_raw_request(self, request_json: str) -> Optional[str]:
        """Send raw JSON request and return raw response."""
        if not self.socket:
            print("❌ Not connected to server")
            return None

        try:
            print(f"📤 Sending raw: {request_json}")
            bytes_sent = self.socket.send((request_json + "\n").encode('utf-8'))
            print(f"📤 Sent {bytes_sent} bytes")

            # Receive response
            response_data = ""
            print("📥 Waiting for response...")
            start_time = time.time()

            while True:
                try:
                    chunk = self.socket.recv(4096).decode('utf-8')
                    if not chunk:
                        print("❌ Connection closed by server")
                        break
                    response_data += chunk
                    print(f"📥 Raw chunk: {repr(chunk)}")

                    # Look for complete JSON response (newline terminated)
                    if '\n' in response_data:
                        response_line = response_data.split('\n')[0]
                        print(f"📥 Complete raw response: {response_line}")
                        return response_line

                except socket.timeout:
                    elapsed = time.time() - start_time
                    print(f"⏱️  Socket timeout after {elapsed:.1f}s")
                    if elapsed > 15:  # Total timeout
                        break

            print("❌ No complete response received")
            print(f"📥 Partial response data: {repr(response_data)}")
            return response_data if response_data else None

        except Exception as e:
            print(f"❌ Request failed: {e}")
            return None

    def analyze_json_field(self, json_obj: Dict, field_path: str) -> Dict[str, Any]:
        """Analyze a specific field in JSON response."""
        analysis = {
            "path": field_path,
            "exists": False,
            "type": None,
            "value": None,
            "raw_value": None,
            "is_string_as_bool": False
        }

        try:
            # Navigate to the field
            current = json_obj
            path_parts = field_path.split('.')

            for part in path_parts:
                if isinstance(current, dict) and part in current:
                    current = current[part]
                else:
                    return analysis

            analysis["exists"] = True
            analysis["value"] = current
            analysis["raw_value"] = repr(current)
            analysis["type"] = type(current).__name__

            # Check if this looks like a string serialized as boolean
            if isinstance(current, bool) and current is True:
                analysis["is_string_as_bool"] = True

        except Exception as e:
            analysis["error"] = str(e)

        return analysis

    def test_basic_initialization(self):
        """Test basic initialization request to see JSON structure."""
        print("\n" + "="*60)
        print("🔍 TESTING BASIC INITIALIZATION")
        print("="*60)

        request = {
            "jsonrpc": "2.0",
            "id": "1",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "roots": {"listChanged": True}
                },
                "clientInfo": {
                    "name": "debug-client",
                    "version": "1.0.0"
                }
            }
        }

        request_json = json.dumps(request, indent=2)
        print(f"📤 Request structure:\n{request_json}")

        raw_response = self.send_raw_request(json.dumps(request))
        if not raw_response:
            return False

        print(f"\n📥 Raw response: {raw_response}")

        try:
            response = json.loads(raw_response)
            print(f"📥 Parsed response structure:")
            print(json.dumps(response, indent=2))

            # Analyze key fields that should be strings
            fields_to_check = [
                "jsonrpc",
                "id",
                "result.protocolVersion",
                "result.serverInfo.name",
                "result.serverInfo.version"
            ]

            print(f"\n🔍 Field Analysis:")
            print("-" * 40)

            for field in fields_to_check:
                analysis = self.analyze_json_field(response, field)
                status = "⚠️  SUSPICIOUS" if analysis["is_string_as_bool"] else "✅ OK"
                print(f"{status} {field}:")
                print(f"   Type: {analysis['type']}")
                print(f"   Value: {analysis['raw_value']}")
                if analysis["is_string_as_bool"]:
                    print(f"   🚨 DETECTED: String serialized as boolean true!")
                print()

            return True

        except json.JSONDecodeError as e:
            print(f"❌ JSON decode error: {e}")
            print("🔍 Analyzing raw response character by character:")
            for i, char in enumerate(raw_response[:200]):  # First 200 chars
                print(f"  [{i:3d}] {ord(char):3d} '{char}' {repr(char)}")
            return False

    def test_simple_ping(self):
        """Test simple ping to isolate the issue."""
        print("\n" + "="*60)
        print("🔍 TESTING SIMPLE PING")
        print("="*60)

        request = {
            "jsonrpc": "2.0",
            "id": "ping-test",
            "method": "ping"
        }

        raw_response = self.send_raw_request(json.dumps(request))
        if not raw_response:
            return False

        print(f"📥 Raw ping response: {raw_response}")

        try:
            response = json.loads(raw_response)
            print(f"📥 Parsed ping response:")
            print(json.dumps(response, indent=2))

            # Check if basic fields are correct
            fields_to_check = ["jsonrpc", "id"]
            for field in fields_to_check:
                analysis = self.analyze_json_field(response, field)
                status = "⚠️  ISSUE" if analysis["is_string_as_bool"] else "✅ OK"
                print(f"{status} {field}: {analysis['raw_value']} (type: {analysis['type']})")

            return True

        except json.JSONDecodeError as e:
            print(f"❌ JSON decode error: {e}")
            return False

    def test_tools_list(self):
        """Test tools list to see tool serialization."""
        print("\n" + "="*60)
        print("🔍 TESTING TOOLS LIST")
        print("="*60)

        request = {
            "jsonrpc": "2.0",
            "id": "tools-test",
            "method": "tools/list"
        }

        raw_response = self.send_raw_request(json.dumps(request))
        if not raw_response:
            return False

        print(f"📥 Raw tools response: {raw_response}")

        try:
            response = json.loads(raw_response)
            print(f"📥 Parsed tools response:")
            print(json.dumps(response, indent=2))

            # Check tool structure
            if "result" in response and "tools" in response["result"]:
                tools = response["result"]["tools"]
                if isinstance(tools, list) and len(tools) > 0:
                    print(f"\n🔍 Analyzing first tool:")
                    tool = tools[0]
                    tool_fields = ["name", "description"]
                    for field in tool_fields:
                        if field in tool:
                            value = tool[field]
                            value_type = type(value).__name__
                            is_suspicious = isinstance(value, bool) and value is True
                            status = "⚠️  SUSPICIOUS" if is_suspicious else "✅ OK"
                            print(f"{status} tool.{field}: {repr(value)} (type: {value_type})")

            return True

        except json.JSONDecodeError as e:
            print(f"❌ JSON decode error: {e}")
            return False

    def test_edge_cases(self):
        """Test edge cases that might reveal the serialization bug."""
        print("\n" + "="*60)
        print("🔍 TESTING EDGE CASES")
        print("="*60)

        # Test various string values that might trigger the bug
        test_cases = [
            {"method": "ping", "description": "Empty params"},
            {"method": "initialize", "params": {"test": "hello"}, "description": "Simple string param"},
            {"method": "initialize", "params": {"test": ""}, "description": "Empty string param"},
            {"method": "initialize", "params": {"test": "true"}, "description": "String 'true'"},
            {"method": "initialize", "params": {"test": "false"}, "description": "String 'false'"},
        ]

        for i, test_case in enumerate(test_cases):
            print(f"\n--- Test Case {i+1}: {test_case['description']} ---")

            request = {
                "jsonrpc": "2.0",
                "id": f"edge-{i}",
                "method": test_case["method"]
            }

            if "params" in test_case:
                request["params"] = test_case["params"]

            raw_response = self.send_raw_request(json.dumps(request))
            if raw_response:
                print(f"Raw: {raw_response}")
                try:
                    parsed = json.loads(raw_response)
                    print(f"Parsed: {json.dumps(parsed, indent=2)}")
                except:
                    print("Failed to parse JSON")

    def run_full_debug_suite(self):
        """Run complete debugging suite."""
        print("🚀 Starting JSON Serialization Debug Suite")
        print("="*60)

        if not self.connect():
            return False

        try:
            success = True
            success &= self.test_simple_ping()
            success &= self.test_basic_initialization()
            success &= self.test_tools_list()
            self.test_edge_cases()  # Always run edge cases

            print("\n" + "="*60)
            print("📊 DEBUG SUITE SUMMARY")
            print("="*60)
            if success:
                print("✅ All tests completed - check output above for serialization issues")
            else:
                print("⚠️  Some tests failed - this helps identify the problem area")

            return success

        finally:
            self.disconnect()

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 debug_json_serialization.py <ESP8266_IP>")
        sys.exit(1)

    esp_ip = sys.argv[1]
    print(f"🔍 JSON Serialization Debugger for ESP8266 at {esp_ip}")

    debugger = JSONSerializationDebugger(esp_ip)
    success = debugger.run_full_debug_suite()

    if not success:
        print("\n💡 Next steps:")
        print("1. Check ESP8266 serial output for error messages")
        print("2. Verify the cJSON library is working correctly")
        print("3. Check tinymcp_json.cpp string serialization methods")
        sys.exit(1)
    else:
        print("\n✅ Debugging complete - analyze output above for serialization patterns")

if __name__ == "__main__":
    main()
