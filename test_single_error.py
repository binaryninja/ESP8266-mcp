#!/usr/bin/env python3
"""
Simple single error test for ESP8266 MCP debugging
"""

import socket
import json
import time

def test_unknown_tool():
    """Test unknown tool error case"""
    print("🔍 Testing unknown tool error case...")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10.0)
        sock.connect(("192.168.86.37", 8080))

        # Initialize
        init_msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "1",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {"name": "Test", "version": "1.0"},
                "capabilities": {}
            }
        }) + "\n"

        sock.send(init_msg.encode())
        init_resp = sock.recv(1024)
        print(f"Init: {init_resp.decode().strip()[:50]}...")

        # Send unknown tool request
        error_msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "2",
            "method": "tools/call",
            "params": {
                "name": "unknown_tool",
                "arguments": {}
            }
        }) + "\n"

        print("Sending unknown tool request...")
        sock.send(error_msg.encode())

        print("Waiting for response...")
        start = time.time()

        try:
            response = sock.recv(1024)
            elapsed = time.time() - start
            if response:
                resp_str = response.decode().strip()
                print(f"Response after {elapsed:.2f}s: {resp_str}")
            else:
                print(f"Empty response after {elapsed:.2f}s")
        except socket.timeout:
            elapsed = time.time() - start
            print(f"TIMEOUT after {elapsed:.2f}s")

        sock.close()

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_unknown_tool()
