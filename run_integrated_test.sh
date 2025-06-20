#!/bin/bash

# ESP8266 MCP Integrated Test Runner - Quick Setup Script
# This script helps you run integrated testing with proper environment setup

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
DEFAULT_ESP_IP="192.168.86.30"
DEFAULT_SERIAL_PORT="/dev/ttyUSB0"
DEFAULT_BAUD_RATE="74880"

echo -e "${BLUE}🚀 ESP8266 MCP Integrated Test Runner${NC}"
echo -e "${BLUE}=====================================${NC}"
echo

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check Python dependencies
check_python_deps() {
    echo -e "${YELLOW}🔍 Checking Python dependencies...${NC}"

    if ! command_exists python3; then
        echo -e "${RED}❌ Python 3 is not installed${NC}"
        exit 1
    fi

    python3 -c "import serial, json, socket, threading, queue, time, sys, subprocess, signal, os, argparse" 2>/dev/null || {
        echo -e "${RED}❌ Missing Python dependencies${NC}"
        echo -e "${YELLOW}Installing required packages...${NC}"
        pip3 install pyserial --user
    }

    echo -e "${GREEN}✅ Python dependencies OK${NC}"
}

# Function to check hardware
check_hardware() {
    echo -e "${YELLOW}🔍 Checking hardware connection...${NC}"

    if [ ! -e "$1" ]; then
        echo -e "${RED}❌ Serial port $1 not found${NC}"
        echo -e "${YELLOW}Available ports:${NC}"
        ls -la /dev/ttyUSB* 2>/dev/null || echo "No USB serial ports found"
        exit 1
    fi

    echo -e "${GREEN}✅ Serial port $1 found${NC}"
}

# Function to test ESP8266 connectivity
test_esp_connectivity() {
    echo -e "${YELLOW}🔍 Testing ESP8266 connectivity...${NC}"

    # Test TCP connection
    python3 -c "
import socket
import sys
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    result = sock.connect_ex(('$1', 8080))
    sock.close()
    if result == 0:
        print('✅ ESP8266 MCP server is reachable at $1:8080')
        sys.exit(0)
    else:
        print('❌ Cannot connect to ESP8266 MCP server at $1:8080')
        sys.exit(1)
except Exception as e:
    print(f'❌ Connection test failed: {e}')
    sys.exit(1)
" || {
        echo -e "${RED}❌ ESP8266 not reachable at $1:8080${NC}"
        echo -e "${YELLOW}💡 Make sure:${NC}"
        echo -e "   - ESP8266 is powered on and connected to WiFi"
        echo -e "   - IP address $1 is correct"
        echo -e "   - MCP server is running on port 8080"
        exit 1
    }

    echo -e "${GREEN}✅ ESP8266 connectivity OK${NC}"
}

# Function to show usage
show_usage() {
    echo -e "${PURPLE}📋 Usage:${NC}"
    echo -e "  $0 [ESP8266_IP] [OPTIONS]"
    echo
    echo -e "${PURPLE}📋 Examples:${NC}"
    echo -e "  $0                           # Use default IP ($DEFAULT_ESP_IP)"
    echo -e "  $0 192.168.1.100             # Use custom IP"
    echo -e "  $0 192.168.1.100 --demo      # Run demo mode"
    echo -e "  $0 192.168.1.100 --monitor   # Monitor only (no tests)"
    echo -e "  $0 192.168.1.100 --test-only # Test only (no serial monitor)"
    echo
    echo -e "${PURPLE}📋 Options:${NC}"
    echo -e "  --demo          Run interactive demo"
    echo -e "  --monitor       Monitor ESP8266 serial output only"
    echo -e "  --test-only     Run tests without serial monitoring"
    echo -e "  --help, -h      Show this help message"
    echo -e "  --check         Check setup and connectivity only"
    echo
}

# Parse command line arguments
ESP_IP="$DEFAULT_ESP_IP"
MODE="full"

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    show_usage
    exit 0
fi

# Check if first argument is an IP address
if [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    ESP_IP="$1"
    shift
fi

# Parse remaining options
while [[ $# -gt 0 ]]; do
    case $1 in
        --demo)
            MODE="demo"
            shift
            ;;
        --monitor)
            MODE="monitor"
            shift
            ;;
        --test-only)
            MODE="test-only"
            shift
            ;;
        --check)
            MODE="check"
            shift
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            echo -e "${RED}❌ Unknown option: $1${NC}"
            show_usage
            exit 1
            ;;
    esac
done

echo -e "${BLUE}🎯 Target ESP8266: $ESP_IP${NC}"
echo -e "${BLUE}📡 Serial Port: $DEFAULT_SERIAL_PORT${NC}"
echo -e "${BLUE}⚡ Baud Rate: $DEFAULT_BAUD_RATE${NC}"
echo -e "${BLUE}🔧 Mode: $MODE${NC}"
echo

# Run checks
check_python_deps
check_hardware "$DEFAULT_SERIAL_PORT"
test_esp_connectivity "$ESP_IP"

if [ "$MODE" = "check" ]; then
    echo -e "${GREEN}🎉 All checks passed! Ready for integrated testing.${NC}"
    exit 0
fi

echo -e "${GREEN}🎉 All checks passed! Starting integrated testing...${NC}"
echo

# Set up signal handling for clean exit
cleanup() {
    echo -e "\n${YELLOW}🛑 Cleaning up...${NC}"
    pkill -f "integrated_test_runner.py" 2>/dev/null || true
    pkill -f "demo_integrated_testing.py" 2>/dev/null || true
    echo -e "${GREEN}🏁 Cleanup complete${NC}"
    exit 0
}

trap cleanup SIGINT SIGTERM

# Generate timestamp for log files
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Run based on mode
case $MODE in
    "demo")
        echo -e "${PURPLE}🎭 Running interactive demo...${NC}"
        echo -e "${YELLOW}💡 This will show real-time correlation between ESP8266 logs and client tests${NC}"
        echo
        python3 demo_integrated_testing.py "$ESP_IP"
        ;;

    "monitor")
        echo -e "${PURPLE}🔍 Running monitor-only mode...${NC}"
        echo -e "${YELLOW}💡 Press Ctrl+C to stop monitoring${NC}"
        echo
        python3 integrated_test_runner.py "$ESP_IP" \
            --monitor-only \
            --log-file "monitor_only_${TIMESTAMP}.log"
        ;;

    "test-only")
        echo -e "${PURPLE}🧪 Running test-only mode...${NC}"
        echo
        python3 integrated_test_runner.py "$ESP_IP" \
            --test-only \
            --config integrated_test_config.json \
            --log-file "test_only_${TIMESTAMP}.log"
        ;;

    "full")
        echo -e "${PURPLE}🔄 Running full integrated testing...${NC}"
        echo -e "${YELLOW}💡 You'll see both ESP8266 serial logs and client test output${NC}"
        echo -e "${YELLOW}💡 Press Ctrl+C to stop${NC}"
        echo
        python3 integrated_test_runner.py "$ESP_IP" \
            --config integrated_test_config.json \
            --log-file "integrated_test_${TIMESTAMP}.log"
        ;;

    *)
        echo -e "${RED}❌ Unknown mode: $MODE${NC}"
        exit 1
        ;;
esac

echo -e "${GREEN}🏁 Integrated testing completed${NC}"
