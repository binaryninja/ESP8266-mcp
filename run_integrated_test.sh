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
DEFAULT_ESP_IP=""  # Will be auto-detected
DEFAULT_SERIAL_PORT="/dev/ttyUSB1"
DEFAULT_BAUD_RATE="74880"
ESP_IP_TIMEOUT=30  # Seconds to wait for IP detection

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

# Function to build and flash firmware
build_and_flash() {
    local serial_port=$1
    echo -e "${YELLOW}🔨 Building and flashing firmware...${NC}"

    # Check if ESP-IDF environment is available
    if ! command_exists idf.py; then
        echo -e "${YELLOW}🔧 Setting up ESP-IDF environment...${NC}"
        export IDF_PATH=/home/$USER/esp/ESP8266_RTOS_SDK
        if [ -f "$IDF_PATH/export.sh" ]; then
            source "$IDF_PATH/export.sh"
        else
            echo -e "${RED}❌ ESP8266-RTOS-SDK not found at $IDF_PATH${NC}"
            echo -e "${YELLOW}💡 Please install ESP8266-RTOS-SDK or set IDF_PATH correctly${NC}"
            exit 1
        fi
    fi

    # Build the project
    echo -e "${BLUE}📦 Building firmware...${NC}"
    idf.py build || {
        echo -e "${RED}❌ Build failed${NC}"
        exit 1
    }

    # Flash the firmware
    echo -e "${BLUE}⚡ Flashing firmware to $serial_port...${NC}"
    idf.py -p "$serial_port" flash || {
        echo -e "${RED}❌ Flash failed${NC}"
        exit 1
    }

    echo -e "${GREEN}✅ Firmware built and flashed successfully${NC}"
}

# Function to capture ESP8266 IP address from serial output
capture_esp_ip() {
    local serial_port=$1
    local timeout=$2

    echo -e "${YELLOW}🔍 Monitoring serial output to capture IP address...${NC}"
    echo -e "${BLUE}📡 Waiting up to ${timeout}s for ESP8266 to boot and connect to WiFi...${NC}"

    local captured_ip=""
    local temp_log="/tmp/esp8266_boot_log_$$.txt"

    # Use timeout and python to monitor serial output
    timeout "$timeout" python3 -c "
import serial
import re
import sys
import time

# IP address regex pattern - matches the ESP-IDF log format
ip_pattern = r'got ip:(\d+\.\d+\.\d+\.\d+)'

try:
    ser = serial.Serial('$serial_port', $DEFAULT_BAUD_RATE, timeout=1)
    print('📡 Connected to serial port $serial_port at $DEFAULT_BAUD_RATE baud')
    print('🔍 Looking for IP address in format \"got ip:x.x.x.x\"...')

    start_time = time.time()
    while time.time() - start_time < $timeout:
        if ser.in_waiting:
            try:
                data = ser.read(ser.in_waiting)
                text = data.decode('utf-8', errors='ignore')

                # Print serial output for debugging
                for line in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'):
                    if line.strip():
                        print(f'📡 ESP8266: {line.strip()}')

                        # Search for IP address
                        ip_match = re.search(ip_pattern, line)
                        if ip_match:
                            ip_addr = ip_match.group(1)
                            print(f'🎯 Found IP address: {ip_addr}')
                            with open('$temp_log', 'w') as f:
                                f.write(ip_addr)
                            ser.close()
                            sys.exit(0)

            except Exception as e:
                print(f'⚠️  Serial read error: {e}')

        time.sleep(0.1)

    print('⏰ Timeout waiting for IP address')
    ser.close()
    sys.exit(1)

except Exception as e:
    print(f'❌ Serial connection failed: {e}')
    sys.exit(1)
" && {
        if [ -f "$temp_log" ]; then
            captured_ip=$(cat "$temp_log")
            rm -f "$temp_log"
            echo -e "${GREEN}🎯 Captured ESP8266 IP address: $captured_ip${NC}"
            echo "$captured_ip"
            return 0
        fi
    }

    rm -f "$temp_log"
    echo -e "${RED}❌ Failed to capture IP address from serial output${NC}"
    return 1
}

# Function to test ESP8266 connectivity
test_esp_connectivity() {
    local ip_addr=$1
    echo -e "${YELLOW}🔍 Testing ESP8266 connectivity at $ip_addr...${NC}"

    # Test TCP connection
    python3 -c "
import socket
import sys
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    result = sock.connect_ex(('$ip_addr', 8080))
    sock.close()
    if result == 0:
        print('✅ ESP8266 MCP server is reachable at $ip_addr:8080')
        sys.exit(0)
    else:
        print('❌ Cannot connect to ESP8266 MCP server at $ip_addr:8080')
        sys.exit(1)
except Exception as e:
    print(f'❌ Connection test failed: {e}')
    sys.exit(1)
" || {
        echo -e "${RED}❌ ESP8266 not reachable at $ip_addr:8080${NC}"
        echo -e "${YELLOW}💡 Make sure:${NC}"
        echo -e "   - ESP8266 MCP server started successfully"
        echo -e "   - No firewall blocking port 8080"
        echo -e "   - WiFi connection is stable"
        return 1
    }

    echo -e "${GREEN}✅ ESP8266 connectivity OK${NC}"
    return 0
}

# Function to show usage
show_usage() {
    echo -e "${PURPLE}📋 Usage:${NC}"
    echo -e "  $0 [ESP8266_IP] [OPTIONS]"
    echo
    echo -e "${PURPLE}📋 Examples:${NC}"
    echo -e "  $0                                    # Auto-detect IP from serial console"
    echo -e "  $0 192.168.1.100                     # Use custom IP (skip auto-detection)"
    echo -e "  $0 --demo                             # Run demo mode with auto-detection"
    echo -e "  $0 --monitor                          # Monitor only (no tests)"
    echo -e "  $0 --test-only                        # Test only (no serial monitor)"
    echo -e "  $0 --device /dev/ttyUSB0              # Use custom serial device"
    echo -e "  $0 --build-flash                      # Build, flash, and auto-detect IP"
    echo
    echo -e "${PURPLE}📋 Options:${NC}"
    echo -e "  --demo          Run interactive demo with auto-detection"
    echo -e "  --monitor       Monitor ESP8266 serial output only"
    echo -e "  --test-only     Run tests without serial monitoring"
    echo -e "  --build-flash   Build and flash firmware before testing"
    echo -e "  --device PATH   Specify serial device path (default: $DEFAULT_SERIAL_PORT)"
    echo -e "  --help, -h      Show this help message"
    echo -e "  --check         Check setup and connectivity only"
    echo -e "  --skip-build    Skip build/flash, only auto-detect IP"
    echo
}

# Parse command line arguments
ESP_IP=""
SERIAL_PORT="$DEFAULT_SERIAL_PORT"
MODE="full"
BUILD_FLASH=false
SKIP_BUILD=false

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
        --device)
            if [ -z "$2" ]; then
                echo -e "${RED}❌ --device requires a path argument${NC}"
                exit 1
            fi
            SERIAL_PORT="$2"
            shift 2
            ;;
        --build-flash)
            BUILD_FLASH=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
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

echo -e "${BLUE}🎯 Target ESP8266: ${ESP_IP:-"Auto-detect"}"
echo -e "${BLUE}📡 Serial Port: $SERIAL_PORT${NC}"
echo -e "${BLUE}⚡ Baud Rate: $DEFAULT_BAUD_RATE${NC}"
echo -e "${BLUE}🔧 Mode: $MODE${NC}"
echo -e "${BLUE}🔨 Build/Flash: $BUILD_FLASH${NC}"
echo

# Run basic checks
check_python_deps
check_hardware "$SERIAL_PORT"

# Build and flash if requested
if [ "$BUILD_FLASH" = true ]; then
    build_and_flash "$SERIAL_PORT"
    echo -e "${YELLOW}⏳ Waiting 3 seconds for ESP8266 to restart...${NC}"
    sleep 3
fi

# Auto-detect IP if not provided
if [ -z "$ESP_IP" ] && [ "$SKIP_BUILD" != true ]; then
    echo -e "${YELLOW}🔍 Auto-detecting ESP8266 IP address...${NC}"
    ESP_IP=$(capture_esp_ip "$SERIAL_PORT" "$ESP_IP_TIMEOUT")
    if [ $? -ne 0 ] || [ -z "$ESP_IP" ]; then
        echo -e "${RED}❌ Failed to auto-detect IP address${NC}"
        echo -e "${YELLOW}💡 Try:${NC}"
        echo -e "   - Check WiFi credentials in main/app_main.cpp"
        echo -e "   - Use --build-flash to rebuild with correct WiFi settings"
        echo -e "   - Manually specify IP: $0 192.168.x.x"
        exit 1
    fi
    echo -e "${GREEN}🎯 Using auto-detected IP: $ESP_IP${NC}"
elif [ -z "$ESP_IP" ]; then
    echo -e "${RED}❌ No IP address specified and --skip-build used${NC}"
    echo -e "${YELLOW}💡 Either provide an IP address or remove --skip-build${NC}"
    exit 1
fi

# Test connectivity if not in monitor-only mode
if [ "$MODE" != "monitor" ]; then
    test_esp_connectivity "$ESP_IP" || {
        echo -e "${YELLOW}⚠️  Connectivity test failed, but continuing with requested mode...${NC}"
    }
fi

if [ "$MODE" = "check" ]; then
    echo -e "${GREEN}🎉 All checks passed! ESP8266 IP: $ESP_IP${NC}"
    echo -e "${GREEN}🎉 Ready for integrated testing.${NC}"
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
        python3 demo_integrated_testing.py "$ESP_IP" --device "$SERIAL_PORT"
        ;;

    "monitor")
        echo -e "${PURPLE}🔍 Running monitor-only mode...${NC}"
        echo -e "${YELLOW}💡 Press Ctrl+C to stop monitoring${NC}"
        echo
        python3 integrated_test_runner.py "$ESP_IP" \
            --monitor-only \
            --serial-port "$SERIAL_PORT" \
            --log-file "monitor_only_${TIMESTAMP}.log"
        ;;

    "test-only")
        echo -e "${PURPLE}🧪 Running test-only mode...${NC}"
        echo
        python3 integrated_test_runner.py "$ESP_IP" \
            --test-only \
            --serial-port "$SERIAL_PORT" \
            --config integrated_test_config.json \
            --log-file "test_only_${TIMESTAMP}.log"
        ;;

    "full")
        echo -e "${PURPLE}🔄 Running full integrated testing...${NC}"
        echo -e "${YELLOW}💡 You'll see both ESP8266 serial logs and client test output${NC}"
        echo -e "${YELLOW}💡 Press Ctrl+C to stop${NC}"
        echo
        python3 integrated_test_runner.py "$ESP_IP" \
            --serial-port "$SERIAL_PORT" \
            --config integrated_test_config.json \
            --log-file "integrated_test_${TIMESTAMP}.log"
        ;;

    *)
        echo -e "${RED}❌ Unknown mode: $MODE${NC}"
        exit 1
        ;;
esac

echo -e "${GREEN}🏁 Integrated testing completed${NC}"
