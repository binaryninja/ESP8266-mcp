#!/bin/bash

# ESP8266 MCP Quick Test Script
# This script helps you quickly test your ESP8266 MCP server

set -e  # Exit on any error

echo "🤖 ESP8266 MCP Quick Test Setup"
echo "================================"

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is not installed. Please install Python 3.6 or later."
    exit 1
fi

echo "✅ Python 3 found: $(python3 --version)"

# Check if we're in the right directory
if [ ! -f "test_mcp_client.py" ]; then
    echo "❌ test_mcp_client.py not found. Please run this script from the ESP8266-mcp directory."
    exit 1
fi

echo "✅ Found MCP client test script"

# Test the client syntax
echo "🧪 Testing client syntax..."
if python3 test_client_syntax.py; then
    echo "✅ Client syntax test passed"
else
    echo "❌ Client syntax test failed"
    exit 1
fi

# Get ESP8266 IP address from user
echo ""
echo "📡 ESP8266 Setup"
echo "================"
echo "Make sure your ESP8266 is:"
echo "  1. Flashed with the MCP server firmware"
echo "  2. Connected to WiFi"
echo "  3. Running the MCP server on port 8080"
echo ""

# Try to detect ESP8266 IP if possible
echo "🔍 Trying to detect ESP8266 on network..."
if command -v nmap &> /dev/null; then
    echo "Scanning for devices with port 8080 open..."
    nmap -p 8080 --open $(ip route | grep -E "192\.168\.|10\.|172\." | head -1 | awk '{print $1}') 2>/dev/null | grep -B 4 "8080/tcp open" | grep "Nmap scan report" | head -3
fi

echo ""
read -p "Enter your ESP8266 IP address (e.g., 192.168.1.100): " ESP_IP

# Validate IP format
if [[ ! $ESP_IP =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
    echo "❌ Invalid IP address format"
    exit 1
fi

echo "✅ Using IP address: $ESP_IP"

# Test basic connectivity
echo ""
echo "🔗 Testing basic connectivity..."
if ping -c 1 -W 3 "$ESP_IP" &> /dev/null; then
    echo "✅ ESP8266 is reachable at $ESP_IP"
else
    echo "⚠️  Warning: Cannot ping $ESP_IP (this might be normal if ping is disabled)"
fi

# Test port connectivity
echo "🔌 Testing port 8080 connectivity..."
if command -v nc &> /dev/null; then
    if timeout 5 nc -z "$ESP_IP" 8080 2>/dev/null; then
        echo "✅ Port 8080 is open on $ESP_IP"
    else
        echo "❌ Port 8080 is not accessible on $ESP_IP"
        echo "   Make sure the MCP server is running on the ESP8266"
        exit 1
    fi
elif command -v telnet &> /dev/null; then
    if timeout 5 telnet "$ESP_IP" 8080 2>/dev/null | grep -q "Connected"; then
        echo "✅ Port 8080 is open on $ESP_IP"
    else
        echo "❌ Port 8080 is not accessible on $ESP_IP"
        echo "   Make sure the MCP server is running on the ESP8266"
        exit 1
    fi
else
    echo "⚠️  Cannot test port connectivity (nc or telnet not available)"
    echo "   Proceeding anyway..."
fi

# Show available test options
echo ""
echo "🎯 Available Test Options"
echo "========================"
echo "1. 🧪 Comprehensive Test (recommended)"
echo "2. 🏓 Quick Ping Test"
echo "3. 🎮 Interactive Mode"
echo "4. 📋 Show Expected Output Demo"
echo "5. 🔧 Manual Command"
echo ""

read -p "Choose test option (1-5): " TEST_OPTION

case $TEST_OPTION in
    1)
        echo ""
        echo "🚀 Running comprehensive test..."
        echo "This will test all MCP features including tools and error handling."
        echo "Press Ctrl+C to stop at any time."
        echo ""
        sleep 2
        python3 test_mcp_client.py "$ESP_IP"
        ;;
    2)
        echo ""
        echo "🏓 Running quick ping test..."
        echo "This will test basic connectivity and initialization."
        echo ""
        sleep 1
        python3 test_mcp_client.py "$ESP_IP" --ping-only
        ;;
    3)
        echo ""
        echo "🎮 Starting interactive mode..."
        echo "You can manually test commands. Type 'quit' to exit."
        echo ""
        sleep 1
        python3 test_mcp_client.py "$ESP_IP" --interactive
        ;;
    4)
        echo ""
        echo "📋 Showing expected output demo..."
        echo ""
        python3 demo_expected_output.py
        ;;
    5)
        echo ""
        echo "🔧 Manual command mode"
        echo "Available commands:"
        echo "  python3 test_mcp_client.py $ESP_IP                    # Comprehensive test"
        echo "  python3 test_mcp_client.py $ESP_IP --interactive      # Interactive mode"
        echo "  python3 test_mcp_client.py $ESP_IP --ping-only        # Quick ping test"
        echo "  python3 test_mcp_client.py $ESP_IP 9000               # Custom port"
        echo ""
        ;;
    *)
        echo "❌ Invalid option"
        exit 1
        ;;
esac

echo ""
echo "🎉 Test completed!"
echo ""
echo "📚 Additional Resources:"
echo "  - README_CLIENT_TEST.md  # Detailed documentation"
echo "  - demo_expected_output.py # Expected output examples"
echo "  - test_client_syntax.py  # Client syntax verification"
echo ""
echo "🔧 Troubleshooting:"
echo "  - Check ESP8266 serial monitor for server logs"
echo "  - Verify WiFi connection on ESP8266"
echo "  - Ensure MCP server is listening on port 8080"
echo "  - Check firewall settings if connection fails"
echo ""
echo "Happy testing! 🚀"
