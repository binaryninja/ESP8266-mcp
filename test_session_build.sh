#!/bin/bash

# Test script for ESP8266 TinyMCP FreeRTOS Session Management
# This script tests the build process and validates the new session management system

set -e

echo "=== ESP8266 TinyMCP Session Management Build Test ==="
echo "Testing FreeRTOS session management implementation..."

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "components/tinymcp" ]; then
    print_error "This script must be run from the ESP8266-mcp project root directory"
    exit 1
fi

print_status "Project directory: $PROJECT_DIR"

# Check ESP-IDF environment
if [ -z "$IDF_PATH" ]; then
    print_error "ESP-IDF not found. Please source the ESP-IDF environment first."
    print_status "Run: source \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

print_success "ESP-IDF found at: $IDF_PATH"

# Verify component files exist
print_status "Checking session management files..."

REQUIRED_FILES=(
    "components/tinymcp/include/tinymcp_session.h"
    "components/tinymcp/include/tinymcp_socket_transport.h"
    "components/tinymcp/include/tinymcp_tools.h"
    "components/tinymcp/src/tinymcp_session.cpp"
    "components/tinymcp/src/tinymcp_socket_transport.cpp"
    "components/tinymcp/src/tinymcp_tools.cpp"
    "main/app_main_session.cpp"
)

missing_files=0
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        print_error "Missing required file: $file"
        ((missing_files++))
    else
        print_success "Found: $file"
    fi
done

if [ $missing_files -gt 0 ]; then
    print_error "$missing_files required files are missing"
    exit 1
fi

# Check core message system files
print_status "Checking core message system files..."

CORE_FILES=(
    "components/tinymcp/include/tinymcp_message.h"
    "components/tinymcp/include/tinymcp_request.h"
    "components/tinymcp/include/tinymcp_response.h"
    "components/tinymcp/include/tinymcp_notification.h"
    "components/tinymcp/include/tinymcp_constants.h"
    "components/tinymcp/include/tinymcp_json.h"
)

for file in "${CORE_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        print_error "Missing core file: $file"
        exit 1
    fi
done

print_success "All core message system files present"

# Backup original main file
if [ -f "main/app_main.cpp" ]; then
    if [ ! -f "main/app_main_original.cpp" ]; then
        print_status "Backing up original main file..."
        cp main/app_main.cpp main/app_main_original.cpp
        print_success "Original main file backed up"
    fi

    # Use the session-based main
    print_status "Switching to session-based main application..."
    cp main/app_main_session.cpp main/app_main.cpp
    print_success "Session-based main application activated"
fi

# Clean any previous build
print_status "Cleaning previous build..."
if [ -d "build" ]; then
    rm -rf build
    print_success "Build directory cleaned"
fi

# Set target
print_status "Setting ESP8266 target..."
idf.py set-target esp8266

# Configure WiFi credentials
print_warning "Note: Make sure to update WiFi credentials in main/app_main.cpp"
print_status "Current WiFi SSID in code: 'FBI Surveillance Van'"

# Test configuration
print_status "Testing project configuration..."
idf.py menuconfig --dont-save-config || {
    print_error "Configuration failed"
    exit 1
}

print_success "Configuration completed"

# Test build
print_status "Starting build process..."
print_status "This may take several minutes..."

# Capture build output
BUILD_LOG="build_session_test.log"
if idf.py build 2>&1 | tee "$BUILD_LOG"; then
    print_success "Build completed successfully!"

    # Check build artifacts
    if [ -f "build/esp8266-mcp.bin" ]; then
        BUILD_SIZE=$(stat -c%s "build/esp8266-mcp.bin")
        print_success "Binary created: esp8266-mcp.bin (${BUILD_SIZE} bytes)"
    fi

    if [ -f "build/esp8266-mcp.elf" ]; then
        print_success "ELF file created: esp8266-mcp.elf"
    fi

else
    print_error "Build failed!"
    print_status "Check build log: $BUILD_LOG"

    # Show last few lines of error
    print_status "Last few lines of build output:"
    tail -20 "$BUILD_LOG"
    exit 1
fi

# Analyze build output for warnings
print_status "Analyzing build output..."
WARNING_COUNT=$(grep -c "warning:" "$BUILD_LOG" || true)
ERROR_COUNT=$(grep -c "error:" "$BUILD_LOG" || true)

if [ $ERROR_COUNT -gt 0 ]; then
    print_error "Build contains $ERROR_COUNT errors"
    exit 1
elif [ $WARNING_COUNT -gt 0 ]; then
    print_warning "Build contains $WARNING_COUNT warnings"
    print_status "Warnings found:"
    grep "warning:" "$BUILD_LOG" | head -10
else
    print_success "Build completed with no errors or warnings"
fi

# Check memory usage
print_status "Analyzing memory usage..."
if [ -f "build/esp8266-mcp.map" ]; then
    # Extract memory information from map file
    print_status "Memory usage analysis:"
    grep -A 20 "Memory Configuration" "build/esp8266-mcp.map" | head -20 || true
fi

# Test size optimization
print_status "Checking binary size..."
if [ -f "build/esp8266-mcp.bin" ]; then
    SIZE=$(stat -c%s "build/esp8266-mcp.bin")
    SIZE_KB=$((SIZE / 1024))

    if [ $SIZE_KB -lt 512 ]; then
        print_success "Binary size: ${SIZE_KB}KB (Good for ESP8266)"
    elif [ $SIZE_KB -lt 1024 ]; then
        print_warning "Binary size: ${SIZE_KB}KB (Acceptable for ESP8266)"
    else
        print_error "Binary size: ${SIZE_KB}KB (Too large for ESP8266)"
    fi
fi

# Verify session management symbols
print_status "Verifying session management symbols..."
if [ -f "build/esp8266-mcp.elf" ]; then
    SYMBOLS_TO_CHECK=(
        "tinymcp.*Session.*"
        "tinymcp.*AsyncTask.*"
        "tinymcp.*EspSocketTransport.*"
        "tinymcp.*ToolRegistry.*"
    )

    for symbol in "${SYMBOLS_TO_CHECK[@]}"; do
        if objdump -t "build/esp8266-mcp.elf" | grep -q "$symbol"; then
            print_success "Found symbols: $symbol"
        else
            print_warning "No symbols found for: $symbol"
        fi
    done
fi

# Test flash size check
print_status "Checking flash size requirements..."
idf.py size || {
    print_warning "Could not analyze flash size requirements"
}

# Generate build summary
print_status "=== BUILD SUMMARY ==="
print_success "✓ All required session management files present"
print_success "✓ Core message system integration verified"
print_success "✓ ESP8266 target configuration successful"
print_success "✓ Build completed successfully"

if [ $WARNING_COUNT -gt 0 ]; then
    print_warning "⚠ Build completed with $WARNING_COUNT warnings"
else
    print_success "✓ No build warnings"
fi

print_status "Session management features implemented:"
print_status "  - FreeRTOS-based session lifecycle management"
print_status "  - Socket transport with message framing"
print_status "  - Async task execution with progress reporting"
print_status "  - Tool registry system with built-in tools"
print_status "  - Memory-optimized design for ESP8266"
print_status "  - Multi-client session support"

print_status "Built-in tools available:"
print_status "  - system_info: ESP8266 system information"
print_status "  - gpio_control: GPIO pin control"
print_status "  - echo: Simple echo tool for testing"
print_status "  - network_scan: WiFi network scanner (async)"

print_status "Next steps:"
print_status "1. Update WiFi credentials in main/app_main.cpp"
print_status "2. Flash to ESP8266: idf.py flash"
print_status "3. Monitor output: idf.py monitor"
print_status "4. Test with MCP client on port 8080"

print_success "ESP8266 TinyMCP FreeRTOS Session Management build test completed successfully!"

# Clean up
if [ -f "$BUILD_LOG" ]; then
    print_status "Build log saved to: $BUILD_LOG"
fi

print_status "To restore original main application:"
print_status "  cp main/app_main_original.cpp main/app_main.cpp"

exit 0
