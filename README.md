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
to the model I want to use in my daily work.

This beacon project can be built two ways, with the possibility of
adding more in the future because of some interface abstraction.

## Build Options

### 1. Host Mock Build (Testing)

You can build it for testing on your (Linux) host computer using CMake.
This **mock** build allows the web pages, beacon scheduler, and finite state 
machine to be tested without microcontroller hardware. The mock implementations
simulate hardware interfaces but don't actually transmit RF signals or control
real GPIO pins. This is perfect for:

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
./bin/host-testbench
```

### 2. ESP32 Target Build (Production)

You can build it for ESP32 using the
[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/)
(v5.4.1 or later) build system. I tested it with ESP32-C3. This is the
embedded target I have aimed this beacon toward initially.

**To build for ESP32:**
```bash
cd target
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

## Testing and Development

This project includes comprehensive test suites for validating WSPR
beacon behavior with accelerated time simulation.

### Component Test Suite

Tests the core Scheduler and FSM components with time acceleration for
rapid validation:

```bash
cd tests
mkdir build && cd build
cmake ..
make
./test_runner
```

**Output Example:**
```
[2021-03-09 12:01:40 UTC] 🟢 TX START #1 on 20m (14.097100 MHz) at 23dBm
[2021-03-09 15:06:40 UTC] 🔴 TX END   #1 on 20m (14.097100 MHz) after 110.6s
[2021-03-09 00:01:40 UTC] 🟢 TX START #2 on 40m (7.040100 MHz) at 30dBm
[2021-05-14 20:21:40 UTC] 🟢 TX START #3 on 80m (3.570100 MHz) at 30dBm
```

**Features:**
- **Time acceleration**: 100x-200x speed (4-minute intervals → ~1.2 seconds)
- **UTC timestamps**: Shows accelerated time progression 
- **Band rotation**: Cycles through 20m, 40m, 80m, 160m with appropriate frequencies
- **WSPR compliance**: Validates 110.6-second transmission duration
- **FSM validation**: Tests all state transitions and error conditions

### Real-Time Web UI Test

Demonstrates the beacon with live browser interface and accelerated time:

```bash
cd tests
mkdir webui-build && cd webui-build
cp ../CMakeLists-webui.txt ./CMakeLists.txt
cmake .
make webui_test
./webui_test
```

Then open http://localhost:8080 in your browser.

**Features:**
- **Real-time status updates** every 1 second
- **Visual transmission indicators**: Red border and pulsing "[TRANSMITTING]" during TX
- **Live countdown timer**: Shows time until next transmission 
- **Simulated time display**: UTC clock advancing at 10x speed
- **Dynamic configuration**: Change settings while beacon operates
- **Band/frequency display**: Shows current transmission details

**What You'll See:**
1. **Time advancing** rapidly in the UTC clock display
2. **Countdown timer**: 16h → 4m → 2:15 → 45s → Now
3. **Transmission begins**: Thick red border around status box
4. **Pulsing indicator**: "[TRANSMITTING]" with animation
5. **Browser tab title** changes to "[TX] WSPR Beacon - Home"
6. **Automatic scheduling**: Next transmission countdown starts immediately

### Test Coverage

**Scheduler Tests:**
- Basic WSPR scheduling and timing
- Protocol compliance (even-minute UTC start, 110.6s duration)
- Multi-band operation with frequency rotation
- Transmission cancellation and error recovery

**FSM Tests:**
- Network state transitions (BOOTING → AP_MODE → STA_CONNECTING → READY)
- Transmission states (IDLE → TX_PENDING → TRANSMITTING → IDLE)
- Error state handling and recovery
- State validation and transition guards

**Integration Tests:**
- Complete beacon startup sequence
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

