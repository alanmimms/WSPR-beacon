Project Context: ESP32 WSPR Beacon (Updated)
This document summarizes the current state, requirements, and development history for an advanced ESP32-based WSPR beacon project.

1. High-Level Goal
The primary objective is to create a feature-rich, standalone WSPR beacon using an ESP32 microcontroller and an Si5351 DDS synthesizer. The device will be configurable and controllable via a web interface.

2. Core Software Architecture
Language & Style
Language: C++

Style: camelCase naming, 2-space indentation, no m_ prefixes, and C-style strings/arrays where practical. Filenames use dashes.

Framework & Components
Framework: ESP-IDF

Main Class: BeaconFsm acts as the central context object, owning all major components.

State Machine: A custom, pointer-based state machine has been implemented to replace tinyfsm.

BeaconState is an abstract base class for all states.

BeaconFsm holds a pointer to the currentState and manages transitions via a transitionTo() method.

This design was chosen for its simplicity and readability over the previous template-based approach.

Component Drivers:

si5351: A C++ class-based driver for the Si5351 chip. A single instance is owned by BeaconFsm.

Scheduler: Manages transmission timing and band plans. It receives a reference to the si5351 and Settings objects from BeaconFsm.

WebServer: Provides the web UI for configuration. It receives a reference to the Settings object.

Settings: Manages loading and saving configuration to NVS.

jtencode: A C++ component for encoding WSPR signals.

Design Pattern: Dependency Injection
The project uses dependency injection to manage shared resources. The BeaconFsm class creates and owns the single instances of Si5351 and Settings. It then passes references (Si5351&, Settings&) to other classes (Scheduler, WebServer) that need them. This ensures a single source of truth and avoids object duplication.

3. Detailed Feature Requirements
Wi-Fi and Networking
Dual-Mode Connectivity: Operates in Station Mode (connecting to a configured Wi-Fi) or falls back to a Provisioning AP Mode.

Provisioning AP: Serves a web page to configure Wi-Fi credentials and other beacon settings, which are saved to NVS.

Connection Logic: The state machine handles repeated attempts to connect in Station mode, falling back to Provisioning mode for a 5-minute timeout before retrying.

Time Management
NTP Synchronization: Will be used for precise time synchronization once the beacon is connected to a network.

Time Zone Support: The UI will allow setting a local time zone for scheduling.

Web Interface & Control
Configuration Page: Allows setting all operational parameters (Callsign, Locator, Power, Master Schedule, Band Plan).

Status & Statistics Page: A read-only page to display current status and transmission history.

4. Hardware & System
MCU: ESP32-C3

Synthesizer: Si5351

Partition Table: A custom partition scheme provides a single, large 3MB application partition, foregoing OTA capability.

5. Development History & Key Decisions
The project was initially implemented using the tinyfsm library with the "Curiously Recurring Template Pattern" (CRTP). This led to complex, non-intuitive compiler errors related to template instantiation (this->template enter<...>()).

A major architectural decision was made to abandon tinyfsm and refactor the state machine to a simpler, more conventional pointer-based design. This resolved the compilation issues and significantly improved code readability.

Several compiler warnings, treated as errors (-Wreorder, -Wmissing-field-initializers, -Waddress), were resolved by reordering member variables, fully initializing structs, and using C++ references (&) instead of pointers (*) for function arguments where null is not a valid state.

The Scheduler and WebServer classes were refactored to accept references to the shared si5351 and settings objects, following a dependency injection pattern.
