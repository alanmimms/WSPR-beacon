#!/bin/bash
# WSPR Beacon Build Aliases
# Add this to your ~/.bashrc or source it: source aliases.sh

# Get the directory where this script is located (project root)
WSPR_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ESP32 build aliases
alias wspr-esp32='cd "$WSPR_ROOT/target-esp32"'
alias wspr-build='cd "$WSPR_ROOT/target-esp32" && idf.py build'
alias wspr-flash='cd "$WSPR_ROOT/target-esp32" && idf.py flash'
alias wspr-monitor='cd "$WSPR_ROOT/target-esp32" && idf.py monitor'
alias wspr-clean='cd "$WSPR_ROOT/target-esp32" && idf.py clean'

# Host-mock build aliases  
alias wspr-host='cd "$WSPR_ROOT/host-mock"'
alias wspr-host-build='cd "$WSPR_ROOT/host-mock" && mkdir -p build && cd build && cmake .. && make -j4'
alias wspr-host-test='cd "$WSPR_ROOT/host-mock/build" && ./bin/host-testbench'
alias wspr-host-clean='cd "$WSPR_ROOT/host-mock" && rm -rf build'

# Smart build script aliases
alias wspr='cd "$WSPR_ROOT" && ./build.sh'
alias wspr-help='cd "$WSPR_ROOT" && ./build.sh --help'

# Project navigation aliases
alias wspr-root='cd "$WSPR_ROOT"'
alias wspr-src='cd "$WSPR_ROOT/src"'
alias wspr-include='cd "$WSPR_ROOT/include"'
alias wspr-web='cd "$WSPR_ROOT/web"'

echo "WSPR Beacon aliases loaded! Available commands:"
echo "  wspr-esp32        - Go to ESP32 build directory"  
echo "  wspr-build        - Build ESP32 firmware"
echo "  wspr-flash        - Flash ESP32"
echo "  wspr-monitor      - Monitor ESP32 serial"
echo "  wspr-clean        - Clean ESP32 build"
echo ""
echo "  wspr-host         - Go to host-mock directory"
echo "  wspr-host-build   - Build host-mock"
echo "  wspr-host-test    - Run host-mock test server"
echo "  wspr-host-clean   - Clean host-mock build"
echo ""
echo "  wspr [args]       - Smart build script"
echo "  wspr-help         - Show build script help"
echo ""
echo "  wspr-root         - Go to project root"
echo "  wspr-src          - Go to src/ directory"
echo "  wspr-include      - Go to include/ directory"
echo "  wspr-web          - Go to web/ directory"