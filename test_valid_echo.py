#!/usr/bin/env python3
"""
Simple test for valid echo call to verify working functionality
"""

import socket
import json
import time

def test_valid_echo():
    """Test valid echo call"""
    print("🔍 Testing valid echo call...")

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

        # Send valid echo request
        echo_msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "2",
            "method": "tools/call",
            "params": {
                "name": "echo",
                "arguments": {"text": "Hello Test"}
            }
        }) + "\n"

        print("Sending valid echo request...")
        sock.send(echo_msg.encode())

        print("Waiting for response...")
        start = time.time()

        try:
            response = sock.recv(1024)
            elapsed = time.time() - start
            if response:
                resp_str = response.decode().strip()
                print(f"SUCCESS: Response after {elapsed:.2f}s: {resp_str}")
            else:
                print(f"FAIL: Empty response after {elapsed:.2f}s")
        except socket.timeout:
            elapsed = time.time() - start
            print(f"FAIL: TIMEOUT after {elapsed:.2f}s")

        sock.close()

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_valid_echo()
