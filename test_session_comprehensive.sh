#!/bin/bash

# Comprehensive Test Script for ESP8266 Session Management System
# This script builds the firmware, flashes it, and runs comprehensive tests
# Author: Session Management Test Suite
# Version: 1.0.0

set -e  # Exit on any error

# Configuration
ESP8266_IP=""
ESP8266_PORT="/dev/ttyUSB0"
MCP_PORT="8080"
BUILD_TARGET="esp8266"
VERBOSE=false
FLASH_ONLY=false
TEST_ONLY=false
SPECIFIC_TOOL=""
CONCURRENT_SESSIONS=3
TEST_TIMEOUT=300  # 5 minutes

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_header() {
    echo -e "\n${PURPLE}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${PURPLE}║$(printf '%78s' '' | tr ' ' ' ')║${NC}"
    echo -e "${PURPLE}║$(printf '%*s' $((39 + ${#1}/2)) "$1" | printf '%-78s' "$(cat)")║${NC}"
    echo -e "${PURPLE}║$(printf '%78s' '' | tr ' ' ' ')║${NC}"
    echo -e "${PURPLE}╚══════════════════════════════════════════════════════════════════════════════╝${NC}\n"
}

# Help function
show_help() {
    cat << EOF
ESP8266 Session Management System - Comprehensive Test Suite

USAGE:
    $0 [OPTIONS]

OPTIONS:
    -h, --help              Show this help message
    -i, --ip IP_ADDRESS     ESP8266 IP address for testing
    -p, --port SERIAL_PORT  Serial port for flashing (default: /dev/ttyUSB0)
    -m, --mcp-port PORT     MCP server port (default: 8080)
    -v, --verbose           Enable verbose output
    -f, --flash-only        Only build and flash, don't run tests
    -t, --test-only         Only run tests (skip build/flash)
    -T, --tool TOOL_NAME    Test specific tool only
    -s, --sessions N        Number of concurrent sessions to test (default: 3)
    --timeout SECONDS       Test timeout in seconds (default: 300)

EXAMPLES:
    # Full test suite
    $0 -i 192.168.1.100

    # Build and flash only
    $0 --flash-only

    # Test only (after manual flash)
    $0 --test-only -i 192.168.1.100

    # Test specific tool
    $0 -i 192.168.1.100 -T echo --verbose

    # Quick test with 1 session
    $0 -i 192.168.1.100 -s 1

REQUIREMENTS:
    - ESP-IDF environment properly configured
    - Python 3.6+ with required packages
    - ESP8266 connected via USB
    - ESP8266 connected to WiFi network (for testing)

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -i|--ip)
                ESP8266_IP="$2"
                shift 2
                ;;
            -p|--port)
                ESP8266_PORT="$2"
                shift 2
                ;;
            -m|--mcp-port)
                MCP_PORT="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -f|--flash-only)
                FLASH_ONLY=true
                shift
                ;;
            -t|--test-only)
                TEST_ONLY=true
                shift
                ;;
            -T|--tool)
                SPECIFIC_TOOL="$2"
                shift 2
                ;;
            -s|--sessions)
                CONCURRENT_SESSIONS="$2"
                shift 2
                ;;
            --timeout)
                TEST_TIMEOUT="$2"
                shift 2
                ;;
            *)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."

    # Check ESP-IDF
    if [ -z "$IDF_PATH" ]; then
        log_error "ESP-IDF not configured. Please run: source \$IDF_PATH/export.sh"
        exit 1
    fi

    # Check Python
    if ! command -v python3 &> /dev/null; then
        log_error "Python 3 not found"
        exit 1
    fi

    # Check required Python packages
    if ! python3 -c "import serial, json, socket, threading, asyncio" 2>/dev/null; then
        log_warning "Installing required Python packages..."
        pip3 install pyserial || {
            log_error "Failed to install required Python packages"
            exit 1
        }
    fi

    # Check serial port (if not test-only)
    if [ "$TEST_ONLY" = false ] && [ ! -e "$ESP8266_PORT" ]; then
        log_error "Serial port $ESP8266_PORT not found"
        exit 1
    fi

    # Check ESP8266 IP (if not flash-only)
    if [ "$FLASH_ONLY" = false ] && [ -z "$ESP8266_IP" ]; then
        log_error "ESP8266 IP address required for testing. Use -i option."
        exit 1
    fi

    log_success "Prerequisites check passed"
}

# Build firmware
build_firmware() {
    log_header "BUILDING SESSION MANAGEMENT FIRMWARE"

    # Check if we should use test firmware
    if [ -f "main/test_session_system.cpp" ]; then
        log_info "Building with on-device test framework..."

        # Backup original main if it exists
        if [ -f "main/app_main.cpp" ]; then
            cp main/app_main.cpp main/app_main.cpp.backup
        fi

        # Use test main
        cp main/test_session_system.cpp main/app_main.cpp
    fi

    log_info "Cleaning previous build..."
    idf.py clean

    log_info "Building firmware..."
    if [ "$VERBOSE" = true ]; then
        idf.py build
    else
        idf.py build > build.log 2>&1 || {
            log_error "Build failed. Check build.log for details."
            tail -20 build.log
            exit 1
        }
    fi

    log_success "Firmware built successfully"
}

# Flash firmware
flash_firmware() {
    log_header "FLASHING FIRMWARE TO ESP8266"

    log_info "Flashing firmware to $ESP8266_PORT..."

    if [ "$VERBOSE" = true ]; then
        idf.py -p "$ESP8266_PORT" flash monitor
    else
        idf.py -p "$ESP8266_PORT" flash > flash.log 2>&1 || {
            log_error "Flash failed. Check flash.log for details."
            tail -20 flash.log
            exit 1
        }
    fi

    log_success "Firmware flashed successfully"
}

# Monitor serial output
monitor_serial() {
    log_info "Starting serial monitor for device logs..."

    # Kill any existing monitor processes
    pkill -f "python3.*test_serial.py" || true

    # Start serial monitor in background
    python3 test_serial.py "$ESP8266_PORT" > serial_output.log 2>&1 &
    SERIAL_PID=$!

    log_info "Serial monitor started (PID: $SERIAL_PID)"
    sleep 2  # Give monitor time to start
}

# Wait for ESP8266 to be ready
wait_for_esp_ready() {
    log_info "Waiting for ESP8266 to be ready..."

    local timeout=60
    local count=0

    while [ $count -lt $timeout ]; do
        if ping -c 1 -W 1 "$ESP8266_IP" &> /dev/null; then
            log_success "ESP8266 is responding to ping"
            break
        fi

        sleep 1
        count=$((count + 1))

        if [ $((count % 10)) -eq 0 ]; then
            log_info "Still waiting for ESP8266... ($count/$timeout seconds)"
        fi
    done

    if [ $count -eq $timeout ]; then
        log_error "ESP8266 not responding after $timeout seconds"
        return 1
    fi

    # Wait a bit more for MCP server to start
    log_info "Waiting for MCP server to start..."
    sleep 5

    return 0
}

# Test MCP connection
test_mcp_connection() {
    log_info "Testing MCP server connection..."

    # Simple connection test
    if timeout 10 bash -c "echo > /dev/tcp/$ESP8266_IP/$MCP_PORT" 2>/dev/null; then
        log_success "MCP server is accepting connections"
        return 0
    else
        log_error "MCP server not responding on port $MCP_PORT"
        return 1
    fi
}

# Run session management tests
run_session_tests() {
    log_header "RUNNING SESSION MANAGEMENT TESTS"

    local test_args=""

    if [ "$VERBOSE" = true ]; then
        test_args="$test_args --verbose"
    fi

    if [ -n "$SPECIFIC_TOOL" ]; then
        test_args="$test_args --tool $SPECIFIC_TOOL"
    fi

    test_args="$test_args --port $MCP_PORT --sessions $CONCURRENT_SESSIONS"

    log_info "Running comprehensive session management tests..."

    # Run session management tests
    timeout $TEST_TIMEOUT python3 test_session_management.py "$ESP8266_IP" $test_args || {
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            log_error "Session management tests timed out after $TEST_TIMEOUT seconds"
        else
            log_error "Session management tests failed (exit code: $exit_code)"
        fi
        return $exit_code
    }

    log_success "Session management tests completed"
}

# Run tool tests
run_tool_tests() {
    log_header "RUNNING TOOL TESTS"

    local test_args="--port $MCP_PORT"

    if [ "$VERBOSE" = true ]; then
        test_args="$test_args --verbose"
    fi

    if [ -n "$SPECIFIC_TOOL" ]; then
        test_args="$test_args --tool $SPECIFIC_TOOL"
    fi

    log_info "Running comprehensive tool tests..."

    # Run tool tests
    timeout $TEST_TIMEOUT python3 test_session_tools.py "$ESP8266_IP" $test_args || {
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            log_error "Tool tests timed out after $TEST_TIMEOUT seconds"
        else
            log_error "Tool tests failed (exit code: $exit_code)"
        fi
        return $exit_code
    }

    log_success "Tool tests completed"
}

# Run original MCP client tests
run_legacy_tests() {
    log_header "RUNNING LEGACY MCP CLIENT TESTS"

    log_info "Running original MCP client tests for compatibility..."

    # Run original test client
    timeout $TEST_TIMEOUT python3 test_mcp_client.py "$ESP8266_IP" "$MCP_PORT" || {
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            log_warning "Legacy tests timed out after $TEST_TIMEOUT seconds"
        else
            log_warning "Legacy tests had issues (exit code: $exit_code)"
        fi
        return 0  # Don't fail overall test suite for legacy tests
    }

    log_success "Legacy tests completed"
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."

    # Kill serial monitor if running
    if [ -n "$SERIAL_PID" ]; then
        kill "$SERIAL_PID" 2>/dev/null || true
    fi

    # Kill any background processes
    pkill -f "python3.*test_.*\.py" || true

    # Restore original main if backed up
    if [ -f "main/app_main.cpp.backup" ]; then
        mv main/app_main.cpp.backup main/app_main.cpp
    fi

    log_info "Cleanup completed"
}

# Generate test report
generate_report() {
    log_header "GENERATING TEST REPORT"

    local report_file="test_report_$(date +%Y%m%d_%H%M%S).md"

    cat > "$report_file" << EOF
# ESP8266 Session Management System Test Report

**Date:** $(date)
**ESP8266 IP:** $ESP8266_IP
**MCP Port:** $MCP_PORT
**Test Configuration:**
- Concurrent Sessions: $CONCURRENT_SESSIONS
- Test Timeout: $TEST_TIMEOUT seconds
- Specific Tool: ${SPECIFIC_TOOL:-"All tools"}
- Verbose Mode: $VERBOSE

## Build Information

EOF

    # Add build info if available
    if [ -f "build.log" ]; then
        echo "### Build Log Summary" >> "$report_file"
        echo '```' >> "$report_file"
        tail -20 build.log >> "$report_file"
        echo '```' >> "$report_file"
        echo "" >> "$report_file"
    fi

    # Add serial output if available
    if [ -f "serial_output.log" ]; then
        echo "### Serial Output Summary" >> "$report_file"
        echo '```' >> "$report_file"
        tail -50 serial_output.log >> "$report_file"
        echo '```' >> "$report_file"
        echo "" >> "$report_file"
    fi

    # Add test results
    echo "### Test Results" >> "$report_file"
    echo "" >> "$report_file"
    echo "See console output above for detailed test results." >> "$report_file"
    echo "" >> "$report_file"

    # Add system information
    echo "### System Information" >> "$report_file"
    echo "" >> "$report_file"
    echo "- **Host OS:** $(uname -a)" >> "$report_file"
    echo "- **Python Version:** $(python3 --version)" >> "$report_file"
    echo "- **ESP-IDF Path:** $IDF_PATH" >> "$report_file"
    echo "- **Serial Port:** $ESP8266_PORT" >> "$report_file"
    echo "" >> "$report_file"

    log_success "Test report generated: $report_file"
}

# Main execution function
main() {
    # Set up trap for cleanup
    trap cleanup EXIT

    log_header "ESP8266 SESSION MANAGEMENT SYSTEM TEST SUITE"

    # Parse arguments
    parse_args "$@"

    # Check prerequisites
    check_prerequisites

    # Build and flash if not test-only
    if [ "$TEST_ONLY" = false ]; then
        build_firmware
        flash_firmware

        # If flash-only, exit here
        if [ "$FLASH_ONLY" = true ]; then
            log_success "Build and flash completed successfully!"
            exit 0
        fi
    fi

    # Start serial monitoring
    monitor_serial

    # Wait for ESP8266 to be ready
    if ! wait_for_esp_ready; then
        log_error "ESP8266 not ready for testing"
        exit 1
    fi

    # Test MCP connection
    if ! test_mcp_connection; then
        log_error "MCP server not accessible"
        exit 1
    fi

    # Run test suites
    local test_failures=0

    # Session management tests
    if ! run_session_tests; then
        test_failures=$((test_failures + 1))
    fi

    # Tool tests (only if no specific tool or tool tests requested)
    if [ -z "$SPECIFIC_TOOL" ] || [ "$SPECIFIC_TOOL" != "session_only" ]; then
        if ! run_tool_tests; then
            test_failures=$((test_failures + 1))
        fi
    fi

    # Legacy compatibility tests
    if [ -z "$SPECIFIC_TOOL" ]; then
        run_legacy_tests  # Don't count failures for legacy tests
    fi

    # Generate report
    generate_report

    # Final results
    log_header "TEST SUITE COMPLETED"

    if [ $test_failures -eq 0 ]; then
        log_success "All tests passed! 🎉"
        echo -e "\n${GREEN}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║                            🏆 SUCCESS! 🏆                                    ║${NC}"
        echo -e "${GREEN}║          ESP8266 Session Management System is working perfectly!            ║${NC}"
        echo -e "${GREEN}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"
        exit 0
    else
        log_error "$test_failures test suite(s) failed"
        echo -e "\n${RED}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║                            ❌ FAILURES ❌                                    ║${NC}"
        echo -e "${RED}║              Some tests failed. Check the output above.                      ║${NC}"
        echo -e "${RED}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"
        exit 1
    fi
}

# Show usage if no arguments
if [ $# -eq 0 ]; then
    show_help
    exit 0
fi

# Run main function
main "$@"
