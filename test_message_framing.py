#!/usr/bin/env python3
"""
Message Framing Test Script

This script tests the ESP8266 MCP server's ability to handle various
message framing scenarios that could cause JSON parsing errors.
"""

import socket
import json
import time
import sys

def test_message_framing(host, port=8080):
    """Test various message framing scenarios."""

    print(f"Testing message framing with {host}:{port}")

    # Test cases that could cause parsing issues
    test_cases = [
        {
            "name": "Normal message",
            "data": '{"jsonrpc":"2.0","id":"1","method":"ping"}\n'
        },
        {
            "name": "Large message (might get split)",
            "data": '{"jsonrpc":"2.0","id":"2","method":"echo","params":{"text":"' + 'A' * 1000 + '"}}\n'
        },
        {
            "name": "Multiple messages in one send",
            "data": '{"jsonrpc":"2.0","id":"3","method":"ping"}\n{"jsonrpc":"2.0","id":"4","method":"ping"}\n'
        },
        {
            "name": "Message sent in chunks (simulating network delays)",
            "chunks": [
                '{"jsonrpc":"2.0","id":"5",',
                '"method":"echo","params":',
                '{"text":"chunked message"}}\n'
            ]
        },
        {
            "name": "Unicode characters",
            "data": '{"jsonrpc":"2.0","id":"6","method":"echo","params":{"text":"Hello 🌍 World! 测试"}}\n'
        }
    ]

    for i, test_case in enumerate(test_cases, 1):
        print(f"\n--- Test {i}: {test_case['name']} ---")

        try:
            # Create new connection for each test
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((host, port))

            # Initialize MCP session first
            init_msg = {
                "jsonrpc": "2.0",
                "id": "init",
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "clientInfo": {"name": "FramingTest", "version": "1.0.0"},
                    "capabilities": {"roots": {"listChanged": False}, "sampling": {}}
                }
            }

            sock.send((json.dumps(init_msg) + "\n").encode())
            response = sock.recv(4096).decode()
            if "result" not in response:
                print(f"❌ Failed to initialize: {response}")
                sock.close()
                continue

            print("✅ Session initialized")

            # Send test message(s)
            if "data" in test_case:
                # Single message
                print(f"Sending: {test_case['data'].strip()}")
                sock.send(test_case["data"].encode())

                # Read response
                response = sock.recv(4096).decode()
                print(f"Received: {response.strip()}")

                if "error" in response:
                    print("❌ Server returned error")
                else:
                    print("✅ Message processed successfully")

            elif "chunks" in test_case:
                # Message sent in chunks
                for j, chunk in enumerate(test_case["chunks"]):
                    print(f"Sending chunk {j+1}: {chunk}")
                    sock.send(chunk.encode())
                    time.sleep(0.1)  # Small delay between chunks

                # Read response
                response = sock.recv(4096).decode()
                print(f"Received: {response.strip()}")

                if "error" in response:
                    print("❌ Server returned error")
                else:
                    print("✅ Chunked message processed successfully")

            sock.close()
            time.sleep(0.5)  # Small delay between tests

        except Exception as e:
            print(f"❌ Test failed with exception: {e}")
            try:
                sock.close()
            except:
                pass

def test_rapid_messages(host, port=8080):
    """Test rapid succession of messages."""
    print(f"\n--- Rapid Message Test ---")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((host, port))

        # Initialize
        init_msg = {
            "jsonrpc": "2.0",
            "id": "init",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {"name": "RapidTest", "version": "1.0.0"},
                "capabilities": {"roots": {"listChanged": False}, "sampling": {}}
            }
        }

        sock.send((json.dumps(init_msg) + "\n").encode())
        response = sock.recv(4096).decode()

        if "result" not in response:
            print(f"❌ Failed to initialize: {response}")
            return

        print("✅ Session initialized")

        # Send multiple ping messages rapidly
        for i in range(10):
            ping_msg = {"jsonrpc": "2.0", "id": f"ping_{i}", "method": "ping"}
            sock.send((json.dumps(ping_msg) + "\n").encode())

        print("Sent 10 rapid ping messages")

        # Try to read all responses
        responses_received = 0
        sock.settimeout(2)  # Shorter timeout for this test

        try:
            while responses_received < 10:
                response = sock.recv(4096).decode()
                for line in response.strip().split('\n'):
                    if line.strip():
                        responses_received += 1
                        print(f"Response {responses_received}: {line}")
        except socket.timeout:
            print(f"Timeout after receiving {responses_received} responses")

        if responses_received >= 8:  # Allow some tolerance
            print("✅ Rapid message test passed")
        else:
            print("❌ Rapid message test failed - not all responses received")

        sock.close()

    except Exception as e:
        print(f"❌ Rapid test failed: {e}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_message_framing.py <ESP8266_IP> [port]")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080

    print("🧪 ESP8266 Message Framing Test")
    print("=" * 50)

    test_message_framing(host, port)
    test_rapid_messages(host, port)

    print("\n" + "=" * 50)
    print("🏁 Message framing tests complete")

if __name__ == "__main__":
    main()
