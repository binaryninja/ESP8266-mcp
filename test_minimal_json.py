#!/usr/bin/env python3
"""
Minimal JSON test to isolate basic serialization issues on ESP8266 MCP Server
This test sends the simplest possible requests to identify where serialization breaks
"""

import socket
import json
import time
import sys

def test_minimal_json(host: str, port: int = 8080):
    """Test minimal JSON operations to isolate serialization issues."""

    print(f"🔗 Connecting to {host}:{port}...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((host, port))
        print("✅ Connected")
    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return False

    # Test 1: Absolute minimal request
    print("\n" + "="*50)
    print("TEST 1: Minimal ping request")
    print("="*50)

    minimal_request = '{"jsonrpc":"2.0","id":"1","method":"ping"}'
    print(f"📤 Sending: {minimal_request}")

    try:
        sock.send((minimal_request + "\n").encode())

        response = ""
        start_time = time.time()
        while time.time() - start_time < 5:
            try:
                chunk = sock.recv(1024).decode()
                if not chunk:
                    break
                response += chunk
                if '\n' in response:
                    break
            except socket.timeout:
                continue

        response_line = response.split('\n')[0] if '\n' in response else response
        print(f"📥 Raw response: '{response_line}'")
        print(f"📥 Response bytes: {[hex(ord(c)) for c in response_line[:20]]}")

        if response_line:
            try:
                parsed = json.loads(response_line)
                print(f"📥 Parsed JSON: {json.dumps(parsed, indent=2)}")

                # Check each field type
                for key, value in parsed.items():
                    value_type = type(value).__name__
                    print(f"   {key}: {repr(value)} (type: {value_type})")

                    # Flag suspicious boolean values
                    if isinstance(value, bool) and value is True:
                        print(f"   ⚠️  SUSPICIOUS: '{key}' is boolean True - might be corrupted string")

            except json.JSONDecodeError as e:
                print(f"❌ JSON parse error: {e}")
                print(f"   Trying to identify non-JSON content...")
                for i, char in enumerate(response_line[:50]):
                    if not char.isprintable() or ord(char) > 127:
                        print(f"   Non-printable at pos {i}: {ord(char)} ({hex(ord(char))})")

    except Exception as e:
        print(f"❌ Test 1 failed: {e}")

    # Test 2: Request with string parameter
    print("\n" + "="*50)
    print("TEST 2: Request with string parameter")
    print("="*50)

    param_request = {
        "jsonrpc": "2.0",
        "id": "2",
        "method": "initialize",
        "params": {
            "clientInfo": {
                "name": "test-client",
                "version": "1.0"
            }
        }
    }

    param_json = json.dumps(param_request)
    print(f"📤 Sending: {param_json}")

    try:
        sock.send((param_json + "\n").encode())

        response = ""
        start_time = time.time()
        while time.time() - start_time < 5:
            try:
                chunk = sock.recv(1024).decode()
                if not chunk:
                    break
                response += chunk
                if '\n' in response:
                    break
            except socket.timeout:
                continue

        response_line = response.split('\n')[0] if '\n' in response else response
        print(f"📥 Raw response: '{response_line}'")

        if response_line:
            try:
                parsed = json.loads(response_line)
                print(f"📥 Parsed JSON: {json.dumps(parsed, indent=2)}")

                # Deep check for boolean-as-string issues
                def check_json_recursive(obj, path=""):
                    if isinstance(obj, dict):
                        for key, value in obj.items():
                            current_path = f"{path}.{key}" if path else key
                            if isinstance(value, bool) and value is True:
                                print(f"   ⚠️  SUSPICIOUS: {current_path} = True (likely corrupted string)")
                            elif isinstance(value, str):
                                print(f"   ✅ STRING: {current_path} = {repr(value)}")
                            elif isinstance(value, (dict, list)):
                                check_json_recursive(value, current_path)
                            else:
                                print(f"   📊 {current_path} = {repr(value)} (type: {type(value).__name__})")
                    elif isinstance(obj, list):
                        for i, item in enumerate(obj):
                            check_json_recursive(item, f"{path}[{i}]")

                check_json_recursive(parsed)

            except json.JSONDecodeError as e:
                print(f"❌ JSON parse error: {e}")

    except Exception as e:
        print(f"❌ Test 2 failed: {e}")

    # Test 3: Empty method test
    print("\n" + "="*50)
    print("TEST 3: Invalid method to test error handling")
    print("="*50)

    error_request = '{"jsonrpc":"2.0","id":"3","method":"nonexistent"}'
    print(f"📤 Sending: {error_request}")

    try:
        sock.send((error_request + "\n").encode())

        response = ""
        start_time = time.time()
        while time.time() - start_time < 5:
            try:
                chunk = sock.recv(1024).decode()
                if not chunk:
                    break
                response += chunk
                if '\n' in response:
                    break
            except socket.timeout:
                continue

        response_line = response.split('\n')[0] if '\n' in response else response
        print(f"📥 Raw response: '{response_line}'")

        if response_line:
            try:
                parsed = json.loads(response_line)
                print(f"📥 Parsed JSON: {json.dumps(parsed, indent=2)}")

                # Check error message structure
                if "error" in parsed and isinstance(parsed["error"], dict):
                    error_obj = parsed["error"]
                    for key, value in error_obj.items():
                        value_type = type(value).__name__
                        print(f"   error.{key}: {repr(value)} (type: {value_type})")
                        if isinstance(value, bool) and value is True:
                            print(f"   🚨 ERROR MESSAGE CORRUPTED: error.{key} should be string, got boolean True")

            except json.JSONDecodeError as e:
                print(f"❌ JSON parse error: {e}")

    except Exception as e:
        print(f"❌ Test 3 failed: {e}")

    sock.close()
    print("\n✅ Minimal JSON tests completed")
    print("Look for '⚠️ SUSPICIOUS' and '🚨 ERROR MESSAGE CORRUPTED' markers above")
    return True

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 test_minimal_json.py <ESP8266_IP>")
        print("Example: python3 test_minimal_json.py 192.168.86.30")
        sys.exit(1)

    esp_ip = sys.argv[1]
    print(f"🧪 Minimal JSON Test for ESP8266 at {esp_ip}")

    success = test_minimal_json(esp_ip)

    if success:
        print(f"\n💡 If you see boolean True values where strings should be:")
        print(f"   1. Check cJSON_CreateString() calls in tinymcp_json.cpp")
        print(f"   2. Verify cJSON library is not corrupted")
        print(f"   3. Check if there's a memory corruption issue")
        print(f"   4. Look at ESP8266 serial output for error messages")

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
