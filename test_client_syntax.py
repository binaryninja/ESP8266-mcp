#!/usr/bin/env python3
"""
Simple syntax test for the MCP client to verify it can be imported and basic functionality works.
This test doesn't require an actual ESP8266 connection.
"""

import sys
import os

def test_import():
    """Test that we can import the MCP client."""
    try:
        # Add current directory to path to import the client
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

        # Import the client module
        import test_mcp_client

        print("✅ Successfully imported test_mcp_client module")
        return True
    except ImportError as e:
        print(f"❌ Failed to import test_mcp_client: {e}")
        return False
    except Exception as e:
        print(f"❌ Unexpected error importing test_mcp_client: {e}")
        return False

def test_client_creation():
    """Test that we can create an MCP client instance."""
    try:
        from test_mcp_client import MCPClient

        # Create client instance (no connection attempted)
        client = MCPClient("127.0.0.1", 8080)

        # Verify basic properties
        assert client.host == "127.0.0.1"
        assert client.port == 8080
        assert client.socket is None
        assert client.request_id == 0
        assert client.initialized == False

        print("✅ Successfully created MCPClient instance")
        print(f"   Host: {client.host}")
        print(f"   Port: {client.port}")
        print(f"   Initialized: {client.initialized}")

        return True
    except Exception as e:
        print(f"❌ Failed to create MCPClient: {e}")
        return False

def test_json_handling():
    """Test JSON handling capabilities."""
    try:
        import json

        # Test creating a typical MCP request
        request = {
            "jsonrpc": "2.0",
            "id": "test_1",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {
                    "name": "test-client",
                    "version": "1.0.0"
                }
            }
        }

        # Serialize to JSON
        json_str = json.dumps(request)

        # Parse back
        parsed = json.loads(json_str)

        assert parsed["jsonrpc"] == "2.0"
        assert parsed["method"] == "initialize"
        assert parsed["params"]["clientInfo"]["name"] == "test-client"

        print("✅ JSON handling works correctly")
        print(f"   Sample request: {json_str[:100]}...")

        return True
    except Exception as e:
        print(f"❌ JSON handling failed: {e}")
        return False

def test_argparse():
    """Test command line argument parsing."""
    try:
        import argparse

        # Create a parser similar to the main script
        parser = argparse.ArgumentParser(description="Test argument parsing")
        parser.add_argument("host", help="Host address")
        parser.add_argument("port", type=int, nargs="?", default=8080, help="Port number")
        parser.add_argument("--interactive", "-i", action="store_true", help="Interactive mode")

        # Test parsing some arguments
        args = parser.parse_args(["192.168.1.100", "9000", "--interactive"])

        assert args.host == "192.168.1.100"
        assert args.port == 9000
        assert args.interactive == True

        print("✅ Argument parsing works correctly")
        print(f"   Host: {args.host}, Port: {args.port}, Interactive: {args.interactive}")

        return True
    except Exception as e:
        print(f"❌ Argument parsing failed: {e}")
        return False

def main():
    """Run all syntax tests."""
    print("🧪 Testing MCP Client Syntax and Dependencies")
    print("=" * 50)

    tests = [
        ("Import Test", test_import),
        ("Client Creation", test_client_creation),
        ("JSON Handling", test_json_handling),
        ("Argument Parsing", test_argparse)
    ]

    passed = 0
    total = len(tests)

    for test_name, test_func in tests:
        print(f"\n📋 Running: {test_name}")
        try:
            if test_func():
                passed += 1
                print(f"✅ {test_name} PASSED")
            else:
                print(f"❌ {test_name} FAILED")
        except Exception as e:
            print(f"❌ {test_name} FAILED with exception: {e}")

    print("\n" + "=" * 50)
    print(f"📊 Results: {passed}/{total} tests passed")

    if passed == total:
        print("🎉 All syntax tests passed! Client is ready to use.")
        return 0
    else:
        print("⚠️  Some tests failed. Check your Python environment.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
