#!/bin/bash

# ESP8266-RTOS-SDK Simplified Setup Script for Linux/Ubuntu
# This script installs and configures the ESP8266-RTOS-SDK development environment

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
ESP_HOME="$HOME/esp"
SDK_VERSION="v3.4"

print_header() {
    echo -e "${BLUE}=================================${NC}"
    echo -e "${BLUE}ESP8266-RTOS-SDK Setup Script${NC}"
    echo -e "${BLUE}=================================${NC}"
    echo ""
}

print_step() {
    echo -e "${GREEN}[STEP] $1${NC}"
}

print_info() {
    echo -e "${YELLOW}[INFO] $1${NC}"
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

check_prerequisites() {
    print_step "Checking prerequisites..."

    # Check if running on supported OS
    if ! command -v apt-get &> /dev/null; then
        print_error "This script is designed for Ubuntu/Debian systems with apt-get"
        exit 1
    fi

    # Check internet connection
    if ! ping -c 1 google.com &> /dev/null; then
        print_error "Internet connection required for downloading packages"
        exit 1
    fi

    print_info "Prerequisites check passed"
}

install_system_dependencies() {
    print_step "Installing system dependencies..."

    # Update package list
    sudo apt-get update

    # Install required packages for ESP8266-RTOS-SDK
    sudo apt-get install -y \
        gcc \
        git \
        wget \
        make \
        libncurses-dev \
        flex \
        bison \
        gperf \
        python3 \
        python3-pip \
        python3-setuptools \
        python3-serial \
        python3-cryptography \
        python3-future \
        python3-pyparsing \
        python3-pyelftools \
        cmake \
        ninja-build \
        ccache \
        libffi-dev \
        libssl-dev \
        unzip \
        curl

    print_info "System dependencies installed successfully"
}

create_esp_directory() {
    print_step "Creating ESP development directory..."

    mkdir -p "$ESP_HOME"
    cd "$ESP_HOME"

    print_info "ESP directory created at $ESP_HOME"
}

download_sdk() {
    print_step "Downloading ESP8266-RTOS-SDK..."

    cd "$ESP_HOME"

    # Clone the SDK repository if not already present
    if [ ! -d "ESP8266_RTOS_SDK" ]; then
        print_info "Cloning ESP8266-RTOS-SDK repository..."
        git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
        cd ESP8266_RTOS_SDK

        # Checkout the specified version
        print_info "Checking out version $SDK_VERSION..."
        git checkout $SDK_VERSION
        git submodule update --init --recursive
    else
        print_info "ESP8266-RTOS-SDK already exists"
        cd ESP8266_RTOS_SDK

        # Update to specified version
        print_info "Updating to version $SDK_VERSION..."
        git fetch
        git checkout $SDK_VERSION
        git submodule update --init --recursive
    fi

    print_info "ESP8266-RTOS-SDK downloaded successfully"
}

install_toolchain() {
    print_step "Installing ESP8266 toolchain..."

    cd "$ESP_HOME/ESP8266_RTOS_SDK"

    # Run the official install script
    print_info "Running official ESP8266-RTOS-SDK install script..."
    if [ -f "install.sh" ]; then
        ./install.sh
    else
        print_error "install.sh not found in ESP8266-RTOS-SDK directory"
        print_info "Attempting manual Python requirements installation..."
        python3 -m pip install --user -r requirements.txt
    fi

    print_info "Toolchain installation completed"
}

setup_environment() {
    print_step "Setting up environment variables..."

    cd "$ESP_HOME/ESP8266_RTOS_SDK"

    # Create environment setup script
    cat > "$ESP_HOME/esp8266_env.sh" << EOF
#!/bin/bash
# ESP8266-RTOS-SDK Environment Setup

# Set IDF_PATH
export IDF_PATH="$ESP_HOME/ESP8266_RTOS_SDK"

# Source the ESP8266-RTOS-SDK export script if it exists
if [ -f "\$IDF_PATH/export.sh" ]; then
    source "\$IDF_PATH/export.sh"
fi

echo "ESP8266-RTOS-SDK environment configured!"
echo "IDF_PATH: \$IDF_PATH"

# Check if toolchain is in PATH
if command -v xtensa-lx106-elf-gcc &> /dev/null; then
    echo "Toolchain: \$(xtensa-lx106-elf-gcc --version | head -n1)"
else
    echo "Warning: xtensa-lx106-elf-gcc not found in PATH"
    echo "You may need to install the toolchain manually"
fi
EOF

    chmod +x "$ESP_HOME/esp8266_env.sh"

    # Add to bashrc if not already present
    BASHRC_LINE="source $ESP_HOME/esp8266_env.sh"
    if ! grep -q "$BASHRC_LINE" "$HOME/.bashrc"; then
        echo "" >> "$HOME/.bashrc"
        echo "# ESP8266-RTOS-SDK Environment" >> "$HOME/.bashrc"
        echo "$BASHRC_LINE" >> "$HOME/.bashrc"
        print_info "Environment setup added to ~/.bashrc"
    else
        print_info "Environment setup already in ~/.bashrc"
    fi

    print_info "Environment configuration completed"
}

verify_installation() {
    print_step "Verifying installation..."

    # Source the environment
    source "$ESP_HOME/esp8266_env.sh"

    # Check if SDK is accessible
    if [ -d "$IDF_PATH" ]; then
        print_info "SDK Path: $IDF_PATH"
    else
        print_error "SDK not found at $IDF_PATH"
        return 1
    fi

    # Check if make is available
    if command -v make &> /dev/null; then
        MAKE_VERSION=$(make --version | head -n1)
        print_info "Make: $MAKE_VERSION"
    else
        print_error "Make not found"
        return 1
    fi

    # Check if toolchain is accessible (may not be installed yet)
    if command -v xtensa-lx106-elf-gcc &> /dev/null; then
        TOOLCHAIN_VERSION_OUTPUT=$(xtensa-lx106-elf-gcc --version | head -n1)
        print_info "Toolchain: $TOOLCHAIN_VERSION_OUTPUT"
    else
        print_info "Toolchain not found - this is normal, you may need to install it separately"
        print_info "See: https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/linux-setup.html"
    fi

    print_info "Installation verification completed"
}

test_hello_world() {
    print_step "Testing with Hello World example..."

    # Source the environment
    source "$ESP_HOME/esp8266_env.sh"

    # Copy hello world example to a test directory
    TEST_DIR="$ESP_HOME/test_hello_world"
    if [ -d "$TEST_DIR" ]; then
        rm -rf "$TEST_DIR"
    fi

    if [ -d "$IDF_PATH/examples/get-started/hello_world" ]; then
        cp -r "$IDF_PATH/examples/get-started/hello_world" "$TEST_DIR"
        cd "$TEST_DIR"

        # Try to build the example
        print_info "Attempting to build Hello World example..."
        if make defconfig && make all; then
            print_info "Hello World example built successfully!"
        else
            print_info "Build failed - this is expected if toolchain is not installed"
            print_info "You can install the toolchain later and try building again"
        fi
    else
        print_info "Hello World example not found - skipping build test"
    fi

    print_info "Test completed"
}

print_completion_message() {
    echo ""
    echo -e "${GREEN}=================================${NC}"
    echo -e "${GREEN}Setup completed!${NC}"
    echo -e "${GREEN}=================================${NC}"
    echo ""
    echo -e "${YELLOW}Next steps:${NC}"
    echo "1. Restart your terminal or run: source ~/.bashrc"
    echo "2. If toolchain is not installed, follow the manual installation guide:"
    echo "   https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/linux-setup.html"
    echo "3. Navigate to your ESP8266-MCP project directory"
    echo "4. Build your project with: make defconfig && make all"
    echo "5. Configure serial port with: make menuconfig"
    echo "6. Flash to device with: make flash"
    echo "7. Monitor output with: make monitor"
    echo ""
    echo -e "${YELLOW}Useful commands:${NC}"
    echo "- Source environment: source $ESP_HOME/esp8266_env.sh"
    echo "- Check toolchain: xtensa-lx106-elf-gcc --version"
    echo "- SDK location: $ESP_HOME/ESP8266_RTOS_SDK"
    echo ""
    echo -e "${YELLOW}Toolchain installation (if needed):${NC}"
    echo "If the toolchain is not automatically installed, you can:"
    echo "1. Download from: https://dl.espressif.com/dl/"
    echo "2. Or use esp-idf tools: python3 \$IDF_PATH/tools/idf_tools.py install"
    echo ""
    echo -e "${BLUE}Happy coding with ESP8266!${NC}"
}

# Main execution
main() {
    print_header

    check_prerequisites
    install_system_dependencies
    create_esp_directory
    download_sdk
    install_toolchain
    setup_environment
    verify_installation
    test_hello_world
    print_completion_message
}

# Handle script interruption
trap 'echo -e "\n${RED}Script interrupted!${NC}"; exit 1' INT TERM

# Run main function
main "$@"
