This is an ESP32 based WSPR encoder with additional capabilities to do
other JTEncoder modes some day.

In the `src/jtencoder` directory I started with the GPL'ed JTEncoder
source code and rewrote most of it by [vibe coding with Google Gemini
2.5Pro](https://g.co/gemini/share/7c0f292dc869). I think the result is
simpler and easier to use and understand. I enjoyed using AI to do
this, although it was not without its frustrations.

I'm continuing my AI journey with Claude Code, which is _much_ closer
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

You can build it for ESP32 using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/) 
(v5.4.1 or later) build system. I tested it with ESP32-C3. This is the embedded 
target I have aimed this beacon toward initially.

**To build for ESP32:**
```bash
cd target
idf.py build
idf.py flash
```

### Portability

Given the abstractions I built into the code, you can port this to
almost any microcontroller with a reasonable OS (e.g., FreeRTOS,
Zephyr, etc.) by defining some concrete implementations of the
abstract interfaces and integrating with the target's build system.

Alan Mimms, WB7NAB, 2025
