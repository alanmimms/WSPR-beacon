This is an ESP32 based WSPR encoder with additional capabilities to do
other JTEncoder modes some day.

In the `src/jtencode` directory I started with the GPL'ed JTEncoder
source code and rewrote most of it by [vibe coding with Google Gemini
2.5Pro](https://g.co/gemini/share/7c0f292dc869). I think the result is
simpler and easier to use and understand. I enjoyed using AI to do
this, although it was not without its frustrations.

## JT Encoder Library

The beacon includes a comprehensive WSPR/JT encoding library rewritten for embedded use:

**Supported Modes:**
- **WSPR**: 162 symbols, 1.46 Hz spacing, 683ms periods (~110.6s transmission)
- **FT8**: 79 symbols, 6.25 Hz spacing, 160ms periods  
- **JT65**: 126 symbols, 2.69 Hz spacing, 372ms periods
- **JT9**: 85 symbols, 1.74 Hz spacing, 576ms periods
- **JT4**: 206 symbols, 4.37 Hz spacing, 229ms periods

**Library Structure:**
```
src/jtencode/
├── include/
│   ├── JTEncode.h          # Template-based encoder classes
│   ├── generator.h         # Symbol generation utilities  
│   └── nhash.h            # Hash functions for interleaving
├── JTEncode.cpp           # Main encoder implementations
├── RSEncode.cpp/.h        # Reed-Solomon encoding
├── jtencode-util.cpp/.h   # Utility functions  
├── nhash.c                # Interleaving hash implementation
└── test/                  # Component and integration tests
```

**Key Features:**
- **Template-based design** with compile-time parameters for each mode
- **Complete encoding pipeline**: Message packing → FEC → Interleaving → Symbol generation
- **Protocol compliance** with proper convolutional coding and hash-based interleaving
- **Embedded optimization** with minimal memory footprint and no dynamic allocation
- **Multiple message types** including WSPR Type 1, 2, and 3 messages

**Integration:**
The library is integrated into the host-mock build system and provides:
- **REST API endpoint** (`/api/wspr/encode`) for testing and validation
- **Web interface** (`/wspr-test.html`) for interactive encoding and symbol visualization
- **Beacon integration** ready for actual RF transmission (requires Si5351 symbol modulation)

I also tried Github Copilot. But it kept being limited (couldn't even
look at my Github repo, which is public!) and got stupid after some
number of interactions.

I'm continuing my AI journey with [Claude
Code](https://www.anthropic.com/claude-code), which is _much_ closer
to the model I use in my daily work.

This beacon project can be built two ways, with the possibility of
adding more in the future because of some interface abstraction.

# Features and Requirements
The web user interface must be responsive so it adapts to screen size,
whether desktop, phone, or table browser.

There is a persistent header on each web page presented by the Beacon.
This lists:

* The call sign, grid square, and transmit power (in both dBm and
  watts) being used.

* Centered in the header, an indicator of the beacon's current state:
  active, idle, error, config. When the beacon is active, the beacon's
  current transmit frequency is shown.

* The current time in UTC, which is updated every second in the
  browser view and kept in sync with the Beacon's time of day, which
  itself is kept in sync by period requests to Internet NTP servers.
  
There is a persistent footer at the bottom of each web page that shows

* The node name of the beacon on the Wi-Fi network.

* The time/date of the last beacon reset in UTC.

* The time (in minutes and seconds) until the next transmission and
  the frequency that transmission will use.

* The number of transmissions since last reset.
  
There is a persistent navigation bar on the left side of the display
that shows a set of buttons to switch between the various pages the
Beacon provides for configuration and status.

# Structure of the Code
The code is organized into a `target-esp32` subtree for the target
microcontroller, a `host-mock` subtree for testing the components of
the beacon using the host and mocked interfaces that can operate
without a micrcontroller, and a `src` subtree for the application code
that is common across these two targets. The name `target-esp32` was
chosen to allow for future targeting to other platforms.

## Building

### 1. Host Mock Build (Testing)

The Beacon's code can be built to run on a host Linux machine with
"mock" functions for the hardware components of the beacon like
Si5351, the various target microcontroller functions like
non-volatile storage, etc. This allows a lot of testing and playing
around with the UI without having to flash each time onto the
microcontroller, and it provides a way to do much more automated
testing of the software than running on the target would easily
accomplish.

This **mock** build is perfect for:
- Testing the web interface and settings management
- Verifying beacon scheduling logic and state machine behavior
- Developing and debugging without hardware
- Continuous integration testing

**To build the host mock:**
```bash
cd host-mock
mkdir build && cd build
cmake ..
make
```

#### Running the Mock Server

```bash
./bin/host-testbench --help
```

**Command Line Options:**
```
Usage: ./bin/host-testbench [options]
Options:
  --mock-data <file>        Path to mock data JSON file (default: mock-data.txt)
  --log-file <file>         Path to detailed operation log file (default: stderr only)
  --log-verbosity <config>  Configure logging verbosity per subsystem
                            Format: subsystem.level[,subsystem.level...]
                            Subsystems: API, WIFI, TX, TIME, SETTINGS, SYSTEM, HTTP
                            Levels: none, basic, v/verbose, vv/debug, vvv/trace
                            Examples: --log-verbosity "*.v" (all verbose)
                                      --log-verbosity "API.vv,WIFI.v,TX.basic"
                                      --log-verbosity "TIME.none,*.basic"
  --port <port>             Server port (default: 8080)
  --time-scale <n>          Time acceleration factor (default: 1.0)
                            > 1.0 = faster, < 1.0 = slower
                            e.g., 60 = 1 minute real time = 1 hour mock time
  --help, -h                Show this help message
```

**Example Usage:**
```bash
# Normal speed with default mock data
./bin/host-testbench

# Fast time (60x) to see transmissions quickly
./bin/host-testbench --time-scale 60

# Test with excellent signal strength
./bin/host-testbench --mock-data ../../platform/host-mock/test-excellent-signal.txt

# Slow motion (0.1x) for debugging
./bin/host-testbench --time-scale 0.1 --port 8081

# Test different WiFi modes
./bin/host-testbench --mock-data ../../platform/host-mock/test-sta-mode.txt --time-scale 10
./bin/host-testbench --mock-data ../../platform/host-mock/test-ap-mode.txt --time-scale 10
./bin/host-testbench --mock-data ../../platform/host-mock/test-disconnected.txt

# Test with comprehensive operation logging
./bin/host-testbench --log-file beacon_ops.log --time-scale 60
./bin/host-testbench --mock-data ../../platform/host-mock/test-sta-mode.txt --log-file wifi_test.log --time-scale 10

# Advanced logging examples
./bin/host-testbench --log-verbosity "*.v"                    # All subsystems verbose
./bin/host-testbench --log-verbosity "API.vv,WIFI.v"          # Debug API, verbose WiFi
./bin/host-testbench --log-verbosity "TIME.none,*.basic"      # Silence time events
./bin/host-testbench --log-verbosity "TX.vvv,SCHEDULER.vv"    # Trace TX, debug scheduler
```

Then open http://localhost:8080 in your browser.

#### WSPR Encoder Testing

The host-mock testbench includes a complete WSPR encoder test interface accessible at http://localhost:8080/wspr-test.html

**WSPR Encoder Features:**
- **Complete JT/WSPR encoding library** rewritten for embedded use
- **Real-time encoding testing** with visual symbol display  
- **Protocol compliance verification** with proper timing and spacing
- **Multiple JT mode support**: WSPR, FT8, JT65, JT9, JT4

**Test Interface:**
- **Interactive encoding form** with callsign, locator, power, and frequency inputs
- **Real-time symbol generation** showing all 162 WSPR symbols  
- **Color-coded symbol display** with frequency offset tooltips
- **Export capabilities** for symbols (copy/download JSON)
- **Protocol information** with technical specifications

**API Endpoint:**
```bash
# Test WSPR encoding via REST API
curl -X POST http://localhost:8080/api/wspr/encode \
  -H "Content-Type: application/json" \
  -d '{
    "callsign": "N0CALL",
    "locator": "AA00aa", 
    "powerDbm": 10,
    "frequency": 14097100
  }'
```

**Response includes:**
- Complete 162-symbol array for transmission
- Timing parameters (symbol period, tone spacing)
- Transmission duration calculations
- Frequency offset information

**Features:**
- **Real-time status updates** with configurable time acceleration
- **Dynamic transmission state**: Automatically cycles between IDLE and TRANSMITTING
- **Live countdown timer**: Shows exact seconds until next transmission
- **Bold red "TRANSMITTING"**: Prominent header indicator during transmissions
- **WSPR-compliant timing**: 120-second cycles, ~111-second transmissions
- **WiFi RSSI indicators**: Color-coded signal strength (green/orange/red)
- **Uptime tracking**: Updates every minute showing days/hours/minutes

**What You'll See with Time Acceleration (--time-scale 60):**
1. **UTC clock advancing** at 60x speed (1 real second = 1 mock minute)
2. **Countdown timer** rapidly decreasing: 120s → 60s → 30s → Now
3. **Bold red "TRANSMITTING"** appears in header with pulsing animation
4. **Transmission duration**: ~111 seconds (less than 2 real seconds at 60x)
5. **Statistics updating**: TX count and total minutes increase in real-time
6. **Next transmission** scheduled based on TX percentage setting

**WiFi Mode Testing:**
The mock server simulates both Station (STA) and Access Point (AP) WiFi modes:

- **STA Mode**: Shows "Connected: [SSID]" in footer, displays RSSI signal strength
- **AP Mode**: Shows "AP: N clients" in footer, RSSI field shows "-" (not applicable)
- **WiFi Scan**: Returns realistic networks with varying signal strengths over time
- **Dynamic Updates**: Client counts and RSSI values change during testing

## Comprehensive Logging System

The WSPR beacon includes an advanced logging system with per-subsystem verbosity control for thorough operation verification and debugging.

### Logging Levels

The logging system supports five verbosity levels:

- **`none`** - No logging for this subsystem
- **`basic`** - Essential events only (default)
- **`verbose`** (`.v`) - Detailed operational information  
- **`debug`** (`.vv`) - Debug information with internal state
- **`trace`** (`.vvv`, `.vvvv`) - Complete execution trace

### Subsystems

Available subsystems for logging control:

| Subsystem | Description | Typical Events |
|-----------|-------------|----------------|
| **API** | HTTP REST API requests/responses | Endpoint calls, status codes, response sizes |
| **WIFI** | WiFi operations and networking | Scans, connections, signal strength changes |
| **TX** | Transmission events | Start/stop events, band selection, timing |
| **TIME** | Time synchronization and scaling | NTP sync, accelerated time calculations |
| **SETTINGS** | Configuration management | Field changes, validation, persistence |
| **SYSTEM** | Core system operations | Startup, shutdown, memory, errors |
| **HTTP** | Low-level HTTP server events | Raw requests, connection handling |
| **FSM** | Finite State Machine transitions | State changes, validation guards |
| **SCHEDULER** | Transmission scheduling | Band rotation, timing calculations |
| **NETWORK** | Network interface operations | Connection states, protocol events |

### Host Mock Configuration

Use the `--log-verbosity` command line option with format `subsystem.level`:

```bash
# Examples
./bin/host-testbench --log-verbosity "*.v"                    # All subsystems verbose
./bin/host-testbench --log-verbosity "API.vv,WIFI.v"          # Debug API, verbose WiFi  
./bin/host-testbench --log-verbosity "TIME.none,TX.vvv"       # Silence time, trace TX
./bin/host-testbench --log-verbosity "SYSTEM.basic,*.v"       # Basic system, verbose others
```

**Wildcard Support:** Use `*` to configure all subsystems at once, then override specific ones:
```bash
--log-verbosity "*.vv,TIME.none,HTTP.basic"  # Debug all except time (none) and HTTP (basic)
```

### ESP32 Target Configuration

For ESP32 builds, configure logging at compile time by defining these macros:

```cpp
// In your main ESP32 header or build system:
#define ESP32_PLATFORM              // Enable ESP32 mode
#define LOG_LEVEL_API LogLevel::VERBOSE
#define LOG_LEVEL_WIFI LogLevel::DEBUG  
#define LOG_LEVEL_TX LogLevel::TRACE
#define LOG_LEVEL_TIME LogLevel::NONE
#define LOG_LEVEL_SETTINGS LogLevel::BASIC
#define LOG_LEVEL_SYSTEM LogLevel::VERBOSE
#define LOG_LEVEL_HTTP LogLevel::NONE
#define LOG_LEVEL_FSM LogLevel::BASIC
#define LOG_LEVEL_SCHEDULER LogLevel::DEBUG
#define LOG_LEVEL_NETWORK LogLevel::BASIC
```

**ESP32 Output:** All log messages go to `stderr` which appears on the serial console via USB.

### Log Format

All log entries use a structured format with UTC timestamps:

```
2025-07-04 19:12:57.633 UTC [WIFI:VERBOSE] Scan completed with details | networks_found=6, networks=[MyHomeWiFi(-50dBm,WPA2), GuestNet(-65dBm,Open)], scan_duration=1.2s
2025-07-04 19:13:18.457 UTC [TX] Transmission ended | band=20m
2025-07-04 19:13:18.457 UTC [API] Request: /api/status.json | method=GET, status=200, response_size=580 bytes
2025-07-04 19:13:19.123 UTC [SCHEDULER:DEBUG] Next transmission calculated | current_cycle=1247, next_tx_in=45s, band_rotation=round_robin
```

**Format Components:**
- **Timestamp**: UTC time with millisecond precision
- **Subsystem**: Source of the log entry with optional level indicator
- **Event**: Human-readable event description
- **Data**: Structured key=value pairs with relevant details

### Integration Examples

**Basic Operation Verification:**
```bash
# Minimal logging for production-like testing
./bin/host-testbench --log-verbosity "*.basic"
```

**Development and Debugging:**
```bash
# Full verbose logging for development
./bin/host-testbench --log-file debug.log --log-verbosity "*.vv"
```

**Performance Testing:**
```bash
# High-speed testing with focused logging
./bin/host-testbench --time-scale 100 --log-verbosity "TX.vvv,SCHEDULER.vv,TIME.none"
```

**WiFi Troubleshooting:**
```bash
# Focus on network operations
./bin/host-testbench --log-verbosity "WIFI.vvv,NETWORK.vv,API.basic,*.none"
```

**Mock Data Files Provided:**
- `mock-data.txt`: Default configuration (20% TX, STA mode, -65dBm WiFi)
- `test-excellent-signal.txt`: High activity beacon with excellent WiFi signal
- `test-transmitting.txt`: Starts in transmitting state for testing
- `test-sta-mode.txt`: Station mode connected to "TestNetwork_5G" 
- `test-ap-mode.txt`: Access Point mode with 2 connected clients
- `test-disconnected.txt`: STA mode without WiFi credentials (falls back to AP)

#### Component Testing

The project includes comprehensive test suites for validating WSPR beacon behavior:

```bash
cd tests
mkdir build && cd build
cmake ..
make
./test-runner
```

**Output Example:**
```
[2021-03-09 12:01:40 UTC] 🟢 TX START #1 on 20m (14.097100 MHz) at 23dBm
[2021-03-09 15:06:40 UTC] 🔴 TX END   #1 on 20m (14.097100 MHz) after 110.6s
[2021-03-09 00:01:40 UTC] 🟢 TX START #2 on 40m (7.040100 MHz) at 30dBm
[2021-05-14 20:21:40 UTC] 🟢 TX START #3 on 80m (3.570100 MHz) at 30dBm
```

**Test Coverage:**
- **Scheduler Tests**: WSPR scheduling, protocol compliance, multi-band operation
- **FSM Tests**: State transitions, error handling, validation guards
- **Integration Tests**: Complete beacon startup and operation sequences
- **Time acceleration**: 100x-200x speed for rapid validation

### 2. ESP32 Target Build (Production)

You can build it for ESP32 using the
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/)
(v5.4.1 or later) build system. I tested it with ESP32-C3. This is the
embedded target I have aimed this beacon toward initially.

**To build for ESP32:**
```bash
cd target-esp32
idf.py build
idf.py flash
```

### Time Synchronization

The beacon includes proper time synchronization support:

**ESP32 Target:**
- SNTP (Simple Network Time Protocol) client for automatic time sync
- Tracks accurate boot time from first successful SNTP synchronization  
- All scheduling and timing operations use UTC
- Automatic timezone handling (always UTC for WSPR operations)

**Host Mock:**
- Uses system time (assumed to be synchronized)
- Tracks application start time for reset tracking
- All times displayed in UTC format

The web interface clearly indicates all times are in UTC, which is
essential for proper WSPR operation and coordination with other
stations worldwide.

### Portability

Given the abstractions I built into the code, you can port this to
almost any microcontroller with a reasonable OS (e.g., FreeRTOS,
Zephyr, etc.) by defining some concrete implementations of the
abstract interfaces and integrating with the target's build system.

- Multi-component coordination (Scheduler + FSM + Hardware abstraction)
- Real-time configuration changes during operation
- Time synchronization with accelerated clock

### Logging Format

All tests use enhanced logging with:
- **UTC timestamps**: `[2021-03-09 12:01:40 UTC]`
- **Event types**: 🟢 TX START, 🔴 TX END, ⚙️ FSM state changes
- **Band information**: `20m (14.097100 MHz) at 23dBm`
- **Sequence tracking**: Transmission counts and timing validation

This allows complete visibility into beacon behavior even during
highly accelerated time scenarios.

Alan Mimms, WB7NAB, 2025    
... and several AI friends, including Google Gemini, GitHub Copilot,
and Claude Code.

