This is an ESP32 based WSPR encoder with additional capabilities to do
other JTEncoder modes some day.

In the `src/jtencoder` directory I started with the GPL'ed JTEncoder
source code and rewrote most of it by [vibe coding with Google Gemini
2.5Pro](https://g.co/gemini/share/7c0f292dc869). I think the result is
simpler and easier to use and understand. I enjoyed using AI to do
this, although it was not without its frustrations.

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
  --mock-data <file>  Path to mock data JSON file (default: mock-data.txt)
  --port <port>       Server port (default: 8080)
  --time-scale <n>    Time acceleration factor (default: 1.0)
                      > 1.0 = faster, < 1.0 = slower
                      e.g., 60 = 1 minute real time = 1 hour mock time
  --help, -h          Show this help message
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
```

Then open http://localhost:8080 in your browser.

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

