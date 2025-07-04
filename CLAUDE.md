# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Host Mock Build (Development/Testing)
```bash
cd host-mock
mkdir build && cd build
cmake ..
make -j4

# Run with various options:
./bin/host-testbench  # Normal speed, default mock data
./bin/host-testbench --time-scale 60  # 60x time acceleration for testing
./bin/host-testbench --mock-data ../../platform/host-mock/test-transmitting.txt  # Custom mock data
./bin/host-testbench --help  # See all options

# Server runs on http://localhost:8080
```

### ESP32 Target Build (Production)
```bash
cd target-esp32
idf.py build
idf.py flash
idf.py monitor
```

### Testing
```bash
# Component tests with time acceleration
cd tests
mkdir build && cd build
cmake ..
make
./test-runner

# Web UI tests with live browser interface
cd tests
mkdir webui-build && cd webui-build
cp ../CMakeLists-webui.txt ./CMakeLists.txt
cmake .
make webui-test
./webui-test  # Then open http://localhost:8080
```

## Architecture Overview

### Platform Abstraction
The codebase uses a clean platform abstraction layer to support both ESP32 hardware and host-mock testing:

- **Core Logic** (`src/core/`): Platform-independent beacon logic (Beacon, FSM, Scheduler)
- **Platform Implementations** (`platform/`): Platform-specific implementations of interfaces
- **Interface Definitions** (`include/`): Abstract interfaces that platforms must implement

### Key Components

#### AppContext Pattern
All platform services are injected via `AppContext` struct containing pointers to:
- `TimerIntf* timer` - Timer services with accelerated time support
- `NetworkIntf* net` - WiFi and network operations  
- `SettingsIntf* settings` - Persistent configuration storage
- `WebServerIntf* webServer` - HTTP server for web UI
- `GPIOIntf* gpio` - Hardware GPIO control
- Hardware-specific interfaces (Si5351, NVS, Logger)

#### Core State Machine (FSM)
Two orthogonal state machines:
- **NetworkState**: BOOTING → AP_MODE/STA_CONNECTING → READY/ERROR
- **TransmissionState**: IDLE → TX_PENDING → TRANSMITTING → IDLE

#### WSPR Scheduler
Precise timing implementation:
- Even-minute UTC transmission starts (required by WSPR protocol)
- 110.592-second transmission duration
- Band rotation based on configurable hourly schedules
- Time acceleration support for testing (100x-200x speed)

### Web UI Architecture

#### Real-time Updates
- **Persistent header**: callsign, locator, power, bold red "TRANSMITTING" state, frequency, UTC time with NTP sync status
- **Persistent footer**: node name, uptime, WiFi status/RSSI, next TX countdown, "TX: N Mmins" format
- **Time acceleration support**: Configurable time scaling for testing (1x to 1000x+)
- **Dynamic transmission state**: Automatically cycles between IDLE and TRANSMITTING based on schedule
- **Live countdown timer**: Shows exact seconds until next transmission
- **Color-coded indicators**: Green/orange/red WiFi RSSI, pulsing red transmission state
- 1-second polling for live status updates
- Responsive design with mobile-friendly layout

#### API Endpoints
- `GET /api/settings` - Retrieve configuration
- `POST /api/settings` - Update configuration  
- `GET /api/status.json` - Current status and statistics with dynamic transmission state
- `GET /api/time` - Server time synchronization with NTP sync status
- `GET /api/live-status` - Real-time status (ESP32 only)

#### Mock Server Features
- **External mock data files**: JSON-based configuration for different test scenarios
- **Time acceleration**: `--time-scale N` parameter for accelerated testing
- **Dynamic transmission simulation**: Realistic WSPR cycle timing with 120s periods
- **Accurate statistics**: Proper tracking of transmission count and actual transmission time
- **Multiple test scenarios**: Default, excellent signal, transmitting state configurations

#### JavaScript Architecture
- `header.js` - HeaderManager class for persistent header/footer management with NTP sync tracking
- `home.js` - Status page with simplified transmission statistics (removed totals, band-only display)
- `settings.js` - Configuration management with form validation
- `nav.js` - Navigation between pages

#### UI Improvements (Recent)
- **NTP sync status**: Shows "last NTP sync N minutes" below UTC time in header
- **Larger time display**: 20% larger font for better readability
- **Bold red transmission state**: Prominent "TRANSMITTING" indicator with pulsing animation
- **Full-width footer**: Spans entire page width, no wrapping issues
- **Simplified statistics**: Single-column band layout, removed redundant totals
- **Improved footer format**: "TX: N Mmins" showing transmission count and actual transmission time

## Development Patterns

### Code Style
- camelCase for variables and functions
- 2-space indentation
- C-style string handling for embedded compatibility
- No exceptions (embedded-friendly error handling)

### Time Handling
- UTC everywhere - all times are UTC, never local time
- Unix timestamps for internal storage and API
- ISO 8601 format for display (`YYYY-MM-DD HH:MM:SS UTC`)
- SNTP synchronization on ESP32, system time on host-mock

### Testing Strategy
- **Component isolation**: Each component (Scheduler, FSM) has standalone tests
- **Time acceleration**: 100x-200x speed for rapid protocol validation
- **Mock implementations**: Full hardware simulation for host testing
- **Integration tests**: Complete beacon behavior validation

### Configuration Management
- JSON-based settings with schema validation
- Band-specific configuration (frequency, schedule, enabled/disabled)
- Web UI for real-time configuration changes
- Persistent storage via NVS (ESP32) or JSON files (host-mock)

## Important Implementation Details

### JT Encoding
The `src/jtencode/` directory contains a rewritten WSPR encoder optimized for embedded use. It supports multiple JT modes and has been extensively tested for protocol compliance.

### Hardware Abstraction
Key interfaces that must be implemented for new platforms:
- `TimerIntf` - Must support one-shot and periodic timers
- `NetworkIntf` - WiFi client and AP mode support required
- `SettingsIntf` - Persistent key-value storage
- `Si5351Intf` - RF synthesizer control for frequency generation

### Error Recovery
The FSM includes comprehensive error handling:
- Network failures trigger AP mode fallback
- Invalid settings cause graceful degradation
- Hardware errors are logged and reported via web UI

### WSPR Protocol Compliance
Critical timing requirements:
- Transmissions must start at even UTC minutes (00 or 02 seconds past)
- Exact 110.592-second duration (50.8 seconds preamble + 110.592 seconds data)
- Frequency accuracy requirements met by Si5351 calibration