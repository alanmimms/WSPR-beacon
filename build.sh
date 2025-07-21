#!/bin/bash

# Smart build script for WSPR Beacon project
# Automatically detects target and switches to correct directory

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
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

# Function to show usage
show_usage() {
    cat << EOF
WSPR Beacon Smart Build Script

Usage: $0 [TARGET] [COMMAND] [OPTIONS...]

TARGETS:
    esp32      Build for ESP32 hardware (default)
    host       Build for host-mock testing
    both       Build both targets

COMMANDS:
    build      Build the project (default)
    clean      Clean build files
    flash      Flash to ESP32 (ESP32 target only)
    monitor    Monitor ESP32 serial output (ESP32 target only)
    test       Run tests (host target only)

OPTIONS:
    -v, --verbose    Verbose output
    -j N             Parallel build jobs (default: 4)
    --help           Show this help

EXAMPLES:
    $0                    # Build ESP32 target
    $0 esp32 flash        # Build and flash ESP32
    $0 host test          # Build host and run tests  
    $0 both               # Build both targets
    $0 esp32 clean build  # Clean then build ESP32

NOTE: This script automatically changes to the correct directory,
      so you can run it from anywhere in the project.
EOF
}

# Parse arguments
TARGET=""
COMMANDS=()
PARALLEL_JOBS=4
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        esp32|host|both)
            TARGET="$1"
            shift
            ;;
        build|clean|flash|monitor|test)
            COMMANDS+=("$1")
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo
            show_usage
            exit 1
            ;;
    esac
done

# Default values
if [[ -z "$TARGET" ]]; then
    TARGET="esp32"
fi

if [[ ${#COMMANDS[@]} -eq 0 ]]; then
    COMMANDS=("build")
fi

# Find project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

print_info "WSPR Beacon Build Script"
print_info "Project root: $PROJECT_ROOT"
print_info "Target: $TARGET"
print_info "Commands: ${COMMANDS[*]}"

# Function to build ESP32
build_esp32() {
    local cmd="$1"
    
    print_info "Building ESP32 target..."
    cd "$PROJECT_ROOT/target-esp32"
    
    # Initialize ESP-IDF environment if not already done
    if [[ -z "$IDF_PATH" ]]; then
        print_warning "ESP-IDF environment not initialized"
        print_info "Run: source ~/esp/esp-idf/export.sh"
        print_info "Or add to your ~/.bashrc: alias get_idf='. ~/esp/esp-idf/export.sh'"
        exit 1
    fi
    
    case "$cmd" in
        clean)
            print_info "Cleaning ESP32 build..."
            idf.py clean
            ;;
        build)
            print_info "Building ESP32..."
            if [[ "$VERBOSE" == "true" ]]; then
                idf.py build -v
            else
                idf.py build
            fi
            ;;
        flash)
            print_info "Flashing ESP32..."
            idf.py flash
            ;;
        monitor)
            print_info "Starting monitor..."
            idf.py monitor
            ;;
        *)
            print_error "Unknown ESP32 command: $cmd"
            exit 1
            ;;
    esac
}

# Function to build host-mock
build_host() {
    local cmd="$1"
    
    print_info "Building host-mock target..."
    cd "$PROJECT_ROOT/host-mock"
    
    case "$cmd" in
        clean)
            print_info "Cleaning host build..."
            rm -rf build
            ;;
        build)
            print_info "Building host-mock..."
            mkdir -p build
            cd build
            cmake ..
            make -j"$PARALLEL_JOBS"
            ;;
        test)
            print_info "Running host tests..."
            if [[ ! -f "build/bin/host-testbench" ]]; then
                print_warning "host-testbench not found, building first..."
                build_host build
            fi
            cd build
            print_info "Starting test server at http://localhost:8080"
            print_info "Press Ctrl+C to stop"
            ./bin/host-testbench
            ;;
        *)
            print_error "Unknown host command: $cmd"
            exit 1
            ;;
    esac
}

# Execute commands
for cmd in "${COMMANDS[@]}"; do
    case "$TARGET" in
        esp32)
            build_esp32 "$cmd"
            ;;
        host)
            build_host "$cmd"
            ;;
        both)
            if [[ "$cmd" == "clean" ]]; then
                build_esp32 "$cmd" || true  # Continue even if ESP32 clean fails
                build_host "$cmd" || true
            elif [[ "$cmd" == "build" ]]; then
                print_info "Building both targets..."
                build_host "$cmd"
                print_success "Host build completed"
                build_esp32 "$cmd"
                print_success "ESP32 build completed"
            else
                print_error "Command '$cmd' not supported for 'both' target"
                exit 1
            fi
            ;;
        *)
            print_error "Unknown target: $TARGET"
            exit 1
            ;;
    esac
done

print_success "All commands completed successfully!"

# Return to original directory
cd "$PROJECT_ROOT"