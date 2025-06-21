#!/bin/bash

# ESP8266 MCP Quick Build, Flash, and Test Script
# This script builds the firmware, flashes it, captures the IP, and runs tests

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
SERIAL_PORT="/dev/ttyUSB1"
BAUD_RATE="74880"
IP_TIMEOUT=30

echo -e "${BLUE}🚀 ESP8266 MCP Quick Build & Test${NC}"
echo -e "${BLUE}=================================${NC}"
echo

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to setup ESP-IDF environment
setup_esp_idf() {
    echo -e "${YELLOW}🔧 Setting up ESP-IDF environment...${NC}"

    if command_exists idf.py; then
        echo -e "${GREEN}✅ idf.py already available${NC}"
        return 0
    fi

    export IDF_PATH=/home/$USER/esp/ESP8266_RTOS_SDK
    if [ -f "$IDF_PATH/export.sh" ]; then
        source "$IDF_PATH/export.sh"
        echo -e "${GREEN}✅ ESP-IDF environment loaded${NC}"
    else
        echo -e "${RED}❌ ESP8266-RTOS-SDK not found at $IDF_PATH${NC}"
        echo -e "${YELLOW}💡 Please install ESP8266-RTOS-SDK first${NC}"
        exit 1
    fi
}

# Function to build firmware
build_firmware() {
    echo -e "${YELLOW}📦 Building firmware...${NC}"
    idf.py build || {
        echo -e "${RED}❌ Build failed${NC}"
        exit 1
    }
    echo -e "${GREEN}✅ Build successful${NC}"
}

# Function to flash firmware
flash_firmware() {
    echo -e "${YELLOW}⚡ Flashing firmware to $SERIAL_PORT...${NC}"

    # Check if device exists
    if [ ! -e "$SERIAL_PORT" ]; then
        echo -e "${RED}❌ Serial port $SERIAL_PORT not found${NC}"
        echo -e "${YELLOW}Available ports:${NC}"
        ls -la /dev/ttyUSB* 2>/dev/null || echo "No USB serial ports found"
        exit 1
    fi

    idf.py -p "$SERIAL_PORT" flash || {
        echo -e "${RED}❌ Flash failed${NC}"
        exit 1
    }
    echo -e "${GREEN}✅ Flash successful${NC}"
}

# Function to capture IP address from boot logs
capture_ip() {
    echo -e "${YELLOW}🔍 Capturing IP address from boot logs...${NC}"
    echo -e "${BLUE}📡 Monitoring serial output for ${IP_TIMEOUT}s...${NC}"

    local temp_log="/tmp/esp8266_ip_$$.txt"

    timeout "$IP_TIMEOUT" python3 -c "
import serial
import re
import time
import sys

# Regex to match ESP-IDF IP log format
ip_pattern = r'got ip:(\d+\.\d+\.\d+\.\d+)'

try:
    ser = serial.Serial('$SERIAL_PORT', $BAUD_RATE, timeout=1)
    print('📡 Connected to $SERIAL_PORT at $BAUD_RATE baud')
    print('🔍 Looking for \"got ip:x.x.x.x\" pattern...')
    print('-' * 50)

    start_time = time.time()
    while time.time() - start_time < $IP_TIMEOUT:
        if ser.in_waiting:
            try:
                data = ser.read(ser.in_waiting)
                text = data.decode('utf-8', errors='ignore')

                for line in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'):
                    line = line.strip()
                    if line:
                        print(f'ESP8266: {line}')

                        # Check for IP address
                        match = re.search(ip_pattern, line)
                        if match:
                            ip = match.group(1)
                            print('-' * 50)
                            print(f'🎯 FOUND IP ADDRESS: {ip}')
                            with open('$temp_log', 'w') as f:
                                f.write(ip)
                            ser.close()
                            sys.exit(0)
            except Exception as e:
                print(f'Serial read error: {e}')

        time.sleep(0.1)

    print('⏰ Timeout - no IP address found')
    ser.close()
    sys.exit(1)

except Exception as e:
    print(f'❌ Serial connection error: {e}')
    sys.exit(1)
" && {
    if [ -f "$temp_log" ]; then
        local captured_ip=$(cat "$temp_log")
        rm -f "$temp_log"
        echo -e "${GREEN}🎯 Captured IP: $captured_ip${NC}"
        echo "$captured_ip"
        return 0
    fi
}

    rm -f "$temp_log" 2>/dev/null
    echo -e "${RED}❌ Failed to capture IP address${NC}"
    return 1
}

# Function to test connectivity
test_connectivity() {
    local ip=$1
    echo -e "${YELLOW}🔍 Testing connectivity to $ip:8080...${NC}"

    python3 -c "
import socket
import sys

try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    result = sock.connect_ex(('$ip', 8080))
    sock.close()

    if result == 0:
        print('✅ MCP server is responding')
        sys.exit(0)
    else:
        print('❌ MCP server not responding')
        sys.exit(1)

except Exception as e:
    print(f'❌ Connection test failed: {e}')
    sys.exit(1)
" && {
    echo -e "${GREEN}✅ Connectivity test passed${NC}"
    return 0
} || {
    echo -e "${YELLOW}⚠️  Connectivity test failed${NC}"
    return 1
}
}

# Function to run diagnostic test
run_diagnostic() {
    local ip=$1
    echo -e "${YELLOW}🧪 Running JSON corruption diagnostic...${NC}"

    if [ -f "debug_json_corruption.py" ]; then
        python3 debug_json_corruption.py "$ip" --test all
    else
        echo -e "${YELLOW}⚠️  debug_json_corruption.py not found, skipping detailed diagnostic${NC}"
    fi
}

# Function to run basic MCP test
run_basic_test() {
    local ip=$1
    echo -e "${YELLOW}🧪 Running basic MCP test...${NC}"

    if [ -f "test_mcp_client.py" ]; then
        python3 test_mcp_client.py "$ip"
    else
        echo -e "${YELLOW}⚠️  test_mcp_client.py not found, running simple connectivity test${NC}"
        test_connectivity "$ip"
    fi
}

# Main execution
main() {
    echo -e "${BLUE}Starting quick build and test workflow...${NC}"
    echo

    # Step 1: Setup environment
    setup_esp_idf

    # Step 2: Build firmware
    build_firmware

    # Step 3: Flash firmware
    flash_firmware

    # Step 4: Wait for boot
    echo -e "${YELLOW}⏳ Waiting 3 seconds for ESP8266 to restart...${NC}"
    sleep 3

    # Step 5: Capture IP
    ESP_IP=$(capture_ip)
    if [ $? -ne 0 ] || [ -z "$ESP_IP" ]; then
        echo -e "${RED}❌ Could not capture IP address${NC}"
        echo -e "${YELLOW}💡 Check WiFi credentials in main/app_main.cpp${NC}"
        exit 1
    fi

    echo
    echo -e "${GREEN}🎉 ESP8266 is running at IP: $ESP_IP${NC}"
    echo

    # Step 6: Test connectivity
    test_connectivity "$ESP_IP"

    # Step 7: Run tests
    echo
    echo -e "${PURPLE}🧪 Running tests...${NC}"
    run_diagnostic "$ESP_IP"
    echo
    run_basic_test "$ESP_IP"

    echo
    echo -e "${GREEN}🏁 Quick build and test completed!${NC}"
    echo -e "${GREEN}🎯 ESP8266 IP: $ESP_IP${NC}"
    echo -e "${GREEN}📡 Serial Port: $SERIAL_PORT${NC}"
}

# Handle interruption
cleanup() {
    echo -e "\n${YELLOW}🛑 Interrupted by user${NC}"
    exit 1
}

trap cleanup SIGINT SIGTERM

# Check for help
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo -e "${PURPLE}Usage: $0${NC}"
    echo
    echo -e "${PURPLE}This script will:${NC}"
    echo -e "  1. Set up ESP-IDF environment"
    echo -e "  2. Build the firmware"
    echo -e "  3. Flash it to the ESP8266"
    echo -e "  4. Monitor boot logs to capture IP address"
    echo -e "  5. Test connectivity"
    echo -e "  6. Run JSON corruption diagnostics"
    echo -e "  7. Run basic MCP tests"
    echo
    echo -e "${PURPLE}Requirements:${NC}"
    echo -e "  - ESP8266 connected to $SERIAL_PORT"
    echo -e "  - ESP8266-RTOS-SDK installed"
    echo -e "  - WiFi credentials configured in main/app_main.cpp"
    echo
    exit 0
fi

# Run main function
main "$@"
