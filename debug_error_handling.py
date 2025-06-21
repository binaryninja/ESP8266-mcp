#!/usr/bin/env python3
"""
Debug script to test ESP8266 MCP error handling cases
Focuses on the specific failing scenarios to understand what's happening
"""

import socket
import json
import time

def test_error_case(host, port, test_name, request):
    """Test a single error case with detailed logging"""
    print(f"\n{'='*50}")
    print(f"🔍 DEBUG: {test_name}")
    print(f"{'='*50}")

    try:
        # Connect
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(15.0)  # Longer timeout for debugging
        sock.connect((host, port))
        print(f"✅ Connected to {host}:{port}")

        # Initialize first
        init_request = {
            "jsonrpc": "2.0",
            "id": "1",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {"name": "Debug-Client", "version": "1.0.0"},
                "capabilities": {"roots": {"listChanged": False}, "sampling": {}}
            }
        }

        init_msg = json.dumps(init_request) + "\n"
        sock.send(init_msg.encode())

        # Get init response
        init_response = sock.recv(4096).decode().strip()
        print(f"✅ Initialized: {init_response[:100]}...")

        # Send the error test request
        request_json = json.dumps(request)
        print(f"📤 Sending: {request_json}")
        sock.send((request_json + "\n").encode())
        print(f"📤 Sent {len(request_json)} bytes")

        # Try to receive response with detailed timing
        print("📥 Waiting for response...")
        start_time = time.time()

        try:
            response = sock.recv(4096)
            elapsed = time.time() - start_time

            if response:
                response_str = response.decode().strip()
                print(f"📥 Response received after {elapsed:.2f}s:")
                print(f"    Length: {len(response_str)} bytes")
                print(f"    Content: {response_str}")

                # Try to parse as JSON
                try:
                    parsed = json.loads(response_str)
                    if "error" in parsed:
                        print(f"✅ Error response received: {parsed['error']}")
                    else:
                        print(f"❓ Unexpected success response: {parsed}")
                except json.JSONDecodeError as e:
                    print(f"❌ Invalid JSON response: {e}")
            else:
                print(f"❌ Empty response after {elapsed:.2f}s")

        except socket.timeout:
            elapsed = time.time() - start_time
            print(f"⏰ Socket timeout after {elapsed:.2f}s - NO RESPONSE")

        sock.close()

    except Exception as e:
        print(f"❌ Exception: {e}")

def main():
    host = "192.168.86.37"
    port = 8080

    print("🔍 ESP8266 MCP Error Handling Debug")
    print("====================================")

    # Test Case 1: Unknown tool
    test_error_case(host, port, "Unknown Tool", {
        "jsonrpc": "2.0",
        "id": "test1",
        "method": "tools/call",
        "params": {
            "name": "nonexistent_tool",
            "arguments": {}
        }
    })

    time.sleep(2)  # Brief pause between tests

    # Test Case 2: Invalid parameters
    test_error_case(host, port, "Invalid Echo Parameters", {
        "jsonrpc": "2.0",
        "id": "test2",
        "method": "tools/call",
        "params": {
            "name": "echo",
            "arguments": {"wrong_param": "test"}
        }
    })

    time.sleep(2)

    # Test Case 3: Valid case for comparison
    test_error_case(host, port, "Valid Echo (Control)", {
        "jsonrpc": "2.0",
        "id": "test3",
        "method": "tools/call",
        "params": {
            "name": "echo",
            "arguments": {"text": "Hello Debug"}
        }
    })

if __name__ == "__main__":
    main()
