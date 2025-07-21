Deep Class Analysis: WSPR Beacon Architecture

  Class Definition Map by Location

  Interface Definitions (include/)

  Abstract Base Classes:
  - TimerIntf, NetworkIntf, SettingsIntf, Si5351Intf, WebServerIntf - Platform abstraction interfaces
  - Beacon, FSM, Scheduler - Core application classes with pure virtual methods

  Data Structures:
  - AppContext - Dependency injection container holding all platform service pointers
  - Band, Settings, TransmissionState - Configuration and state enums/structs

  Core Implementation (src/core/)

  - Beacon - Main application orchestrator (src/core/Beacon.cpp:1)
  - FSM - Dual state machine for network/transmission states (src/core/FSM.cpp:1)
  - Scheduler - WSPR timing controller (src/core/Scheduler.cpp:1)
  - JTEncode - WSPR protocol encoder (src/jtencode/JTEncode.cpp:1)

  ESP32 Platform (platform/esp32/)

  - Timer, Net, Settings, Si5351Wrapper, WebServer, Logger, GPIO, NVS - Hardware implementations
  - Each implements corresponding *Intf abstract interface

  Host-Mock Platform (platform/host-mock/)

  - Parallel implementations of same interfaces for testing
  - MockTimer, test server with time acceleration

  Build Target Usage

  Host-Mock Build

  Included Classes:
  - Core: Beacon, FSM, Scheduler, JTEncode
  - Platform: All platform/host-mock/ implementations
  - External: cJSON, cpp-httplib
  - Test infrastructure with time acceleration

  ESP32 Build

  Included Classes:
  - Core: Same Beacon, FSM, Scheduler, JTEncode
  - Platform: All platform/esp32/ implementations
  - ESP-IDF components: si5351 driver, DNS server
  - Web assets compiled into SPIFFS

  Architecture Violations & Issues

  DRY Principle Violations

  1. Si5351 Duplication:
    - target-esp32/components/si5351/ (low-level driver)
    - platform/esp32/Si5351.cpp (interface wrapper)
    - platform/host-mock/Si5351.cpp (mock implementation)
  2. Settings Code Duplication:
    - ESP32: 250 lines (platform/esp32/Settings.cpp:1)
    - Host-mock: 236 lines (platform/host-mock/Settings.cpp:1)
    - Likely duplicated JSON handling logic

  Platform Abstraction Violations

  1. Direct Platform Calls in Core:
    - printf(), putchar(), fflush() in src/core/Beacon.cpp:915-947
    - time(), gmtime_r() in src/core/Beacon.cpp:56,74,96
    - rand(), srand() in src/core/Beacon.cpp:145-146
  2. Platform-Specific Code in Wrong Places:
    - #ifdef WIFI_SSID in src/core/Beacon.cpp:423-428
    - #ifdef ESP32_PLATFORM in BeaconLogger.cpp

  Missing Abstractions

  - SymbolOutputIntf: No abstraction for WSPR symbol visualization
  - RandomIntf: Direct use of rand() instead of platform abstraction
  - TimeUtilsIntf: Direct use of gmtime_r() bypasses TimeIntf

  Redundancies Found

  1. Timer Implementations:
    - Timer.cpp vs MockTimer.cpp in host-mock
    - Similar timer creation patterns
  2. JSON Handling:
    - Both platforms implement similar JSON parsing/serialization
    - Could be extracted to common base class
  3. HTTP Server Logic:
    - ESP32 and host-mock both implement similar API endpoints
    - Different underlying libraries but similar request handling

  Build System Dependencies

  Shared Dependencies:

  - cJSON library used by both platforms
  - JTEncode library compiled for both targets
  - Core beacon logic identical across platforms

  Platform-Specific Dependencies:

  - ESP32: ESP-IDF framework, FreeRTOS, hardware drivers
  - Host-Mock: Standard C++ libraries, cpp-httplib, POSIX APIs

  Recommendations

  1. Create SymbolOutputIntf to eliminate direct printf usage
  2. Extract common Settings base class to reduce duplication
  3. Add missing platform abstractions (Random, TimeUtils)
  4. Move platform-specific #ifdefs out of core code
  5. Consider shared HTTP endpoint logic between platforms

  The architecture demonstrates good separation of concerns overall, but has several abstraction leaks that weaken the platform independence of
  the core logic.
