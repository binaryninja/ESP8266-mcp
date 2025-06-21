#!/bin/bash

# Quick build script for ESP8266-MCP testing
# This script builds the firmware and shows any errors

set -e

echo "🔨 Quick Build for ESP8266-MCP"
echo "=============================="

# Change to project directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: Not in ESP8266-MCP project directory"
    echo "Please run this script from the ESP8266-MCP root directory"
    exit 1
fi

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo "❌ Error: ESP-IDF not found"
    echo "Please source the ESP-IDF environment first:"
    echo "  . ~/esp/ESP8266_RTOS_SDK/export.sh"
    exit 1
fi

echo "📁 Project directory: $(pwd)"
echo "🔧 ESP-IDF path: $IDF_PATH"

# Clean previous build (optional - comment out for faster builds)
echo "🧹 Cleaning previous build..."
idf.py clean

# ESP8266 RTOS SDK doesn't need set-target
echo "⚙️  ESP8266 RTOS SDK configured..."

# Build the project
echo "🔨 Building firmware..."
time idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo "📦 Firmware ready in: build/"
    echo ""
    echo "Next steps:"
    echo "  🔌 Flash: idf.py -p /dev/ttyUSB1 flash"
    echo "  📡 Monitor: idf.py -p /dev/ttyUSB1 monitor"
    echo "  🚀 Flash + Monitor: idf.py -p /dev/ttyUSB1 flash monitor"
else
    echo ""
    echo "❌ Build failed!"
    exit 1
fi
