#!/usr/bin/env python3
"""
Targeted test to debug cJSON string handling issues on ESP8266
This test isolates the specific cJSON_AddStringToObject problem
"""

import socket
import json
import time
import sys

def test_cjson_strings(host: str, port: int = 8080):
    """Test cJSON string handling with various scenarios."""

    print(f"🔗 Connecting to {host}:{port}...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((host, port))
        print("✅ Connected")
    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return False

    def send_and_receive(request_data):
        """Helper to send request and get response."""
        try:
            request_json = json.dumps(request_data) if isinstance(request_data, dict) else request_data
            print(f"📤 Sending: {request_json}")

            sock.send((request_json + "\n").encode())

            response = ""
            start_time = time.time()
            while time.time() - start_time < 8:
                try:
                    chunk = sock.recv(2048).decode()
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
                    print(f"📥 Parsed: {json.dumps(parsed, indent=2)}")
                    return parsed
                except json.JSONDecodeError as e:
                    print(f"❌ JSON parse error: {e}")
                    print(f"   Response bytes: {[hex(ord(c)) for c in response_line[:30]]}")
                    return None
            else:
                print("❌ No response received")
                return None

        except Exception as e:
            print(f"❌ Send/receive failed: {e}")
            return None

    # Test 1: Minimal string test
    print("\n" + "="*60)
    print("TEST 1: Minimal string fields (jsonrpc, id)")
    print("="*60)

    minimal_test = {
        "jsonrpc": "2.0",
        "id": "test1",
        "method": "ping"
    }

    response1 = send_and_receive(minimal_test)
    if response1:
        # Check if basic string fields are corrupted
        jsonrpc_val = response1.get("jsonrpc")
        id_val = response1.get("id")

        print(f"\n🔍 String field analysis:")
        print(f"   jsonrpc: {repr(jsonrpc_val)} (type: {type(jsonrpc_val).__name__})")
        print(f"   id: {repr(id_val)} (type: {type(id_val).__name__})")

        if jsonrpc_val is True or id_val is True:
            print(f"🚨 CONFIRMED: cJSON_AddStringToObject is corrupting strings!")
        else:
            print(f"✅ Basic string fields working correctly")

    # Test 2: Various string values
    print("\n" + "="*60)
    print("TEST 2: Different string values")
    print("="*60)

    string_tests = [
        ("empty", ""),
        ("short", "hi"),
        ("normal", "hello world"),
        ("long", "this is a longer string to test memory handling"),
        ("special", "test\"with'quotes"),
        ("unicode", "test with émojis 🚀"),
        ("json_like", '{"key":"value"}'),
        ("bool_like", "true"),
        ("null_like", "null"),
        ("number_like", "123")
    ]

    for test_name, test_value in string_tests:
        print(f"\n--- Testing {test_name}: {repr(test_value)} ---")

        test_request = {
            "jsonrpc": "2.0",
            "id": f"str_{test_name}",
            "method": "initialize",
            "params": {
                "clientInfo": {
                    "name": test_value,
                    "version": "1.0"
                }
            }
        }

        response = send_and_receive(test_request)
        if response and "result" in response:
            # Look for our test string in the response
            result = response["result"]
            if isinstance(result, dict) and "serverInfo" in result:
                server_info = result["serverInfo"]
                if "name" in server_info:
                    server_name = server_info["name"]
                    print(f"   Server name returned: {repr(server_name)} (type: {type(server_name).__name__})")
                    if server_name is True:
                        print(f"   🚨 STRING CORRUPTED: Expected string, got boolean True")

    # Test 3: Error message strings
    print("\n" + "="*60)
    print("TEST 3: Error message string handling")
    print("="*60)

    error_test = {
        "jsonrpc": "2.0",
        "id": "error_test",
        "method": "nonexistent_method"
    }

    response3 = send_and_receive(error_test)
    if response3 and "error" in response3:
        error_obj = response3["error"]
        if isinstance(error_obj, dict):
            message = error_obj.get("message")
            print(f"   Error message: {repr(message)} (type: {type(message).__name__})")
            if message is True:
                print(f"   🚨 ERROR MESSAGE CORRUPTED: Expected string, got boolean True")
            else:
                print(f"   ✅ Error message string working correctly")

    # Test 4: Manual JSON to isolate the issue
    print("\n" + "="*60)
    print("TEST 4: Manual JSON formatting")
    print("="*60)

    # Send raw JSON without Python's json.dumps
    manual_json = '{"jsonrpc":"2.0","id":"manual","method":"ping"}'
    print(f"📤 Manual JSON: {manual_json}")

    response4 = send_and_receive(manual_json)
    if response4:
        print(f"   Manual JSON response shows same pattern")

    # Test 5: Incremental complexity
    print("\n" + "="*60)
    print("TEST 5: Incremental complexity test")
    print("="*60)

    complexity_tests = [
        {"jsonrpc": "2.0", "id": "c1", "method": "ping"},
        {"jsonrpc": "2.0", "id": "c2", "method": "ping", "params": {}},
        {"jsonrpc": "2.0", "id": "c3", "method": "ping", "params": {"test": "value"}},
        {"jsonrpc": "2.0", "id": "c4", "method": "initialize", "params": {"clientInfo": {"name": "test"}}}
    ]

    for i, test in enumerate(complexity_tests, 1):
        print(f"\n--- Complexity Level {i} ---")
        response = send_and_receive(test)
        if response:
            # Count boolean True values that should be strings
            def count_suspicious_bools(obj, path=""):
                count = 0
                if isinstance(obj, dict):
                    for key, value in obj.items():
                        current_path = f"{path}.{key}" if path else key
                        if value is True and key in ["jsonrpc", "id", "method", "message", "name", "version"]:
                            print(f"     🚨 {current_path} = True (should be string)")
                            count += 1
                        elif isinstance(value, (dict, list)):
                            count += count_suspicious_bools(value, current_path)
                elif isinstance(obj, list):
                    for j, item in enumerate(obj):
                        count += count_suspicious_bools(item, f"{path}[{j}]")
                return count

            suspicious = count_suspicious_bools(response)
            if suspicious > 0:
                print(f"     Found {suspicious} suspicious boolean True values")

    sock.close()
    print("\n✅ cJSON string debugging complete")
    print("\n💡 Summary:")
    print("   - If you see boolean True where strings should be, the issue is in cJSON_AddStringToObject")
    print("   - Check ESP8266 cJSON library version and implementation")
    print("   - Consider using cJSON_CreateString + cJSON_AddItemToObject instead")
    return True

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 debug_cjson_strings.py <ESP8266_IP>")
        print("Example: python3 debug_cjson_strings.py 192.168.86.30")
        print("\nThis test specifically targets the cJSON_AddStringToObject bug")
        print("where string values are serialized as boolean 'true'")
        sys.exit(1)

    esp_ip = sys.argv[1]
    print(f"🧪 cJSON String Debug Test for ESP8266 at {esp_ip}")
    print("="*60)
    print("This test will help identify exactly where string serialization fails")
    print("="*60)

    success = test_cjson_strings(esp_ip)

    if success:
        print(f"\n🔧 Recommended fixes if strings appear as boolean True:")
        print(f"   1. Replace cJSON_AddStringToObject with:")
        print(f"      cJSON* str_item = cJSON_CreateString(value.c_str());")
        print(f"      cJSON_AddItemToObject(json, key.c_str(), str_item);")
        print(f"   2. Check ESP8266 cJSON library for bugs")
        print(f"   3. Add error checking after cJSON_CreateString calls")
        print(f"   4. Verify memory is not corrupted")

    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
