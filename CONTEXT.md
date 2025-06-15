Project Context: ESP32 WSPR Beacon
This document summarizes the state, requirements, and development history for an advanced ESP32-based WSPR beacon project.

1. High-Level Goal
The primary objective is to create a feature-rich, standalone WSPR beacon using an ESP32 microcontroller and an Si5351 DDS synthesizer. The device will be configurable and controllable via a web interface.

2. Core Software Architecture
Language & Style: C++ with a preference for camelCase naming, 2-space indentation, no m_ prefixes, and C-style strings/arrays where practical. Filenames should use dashes instead of underscores.

Framework: ESP-IDF.

Key Libraries:

jtencode: A custom C++ component for encoding WSPR and other digital modes.

si5351: A custom C++ class-based driver for the Si5351 chip.

tinyfsm: A finite state machine to manage the application's operational states (e.g., Provisioning, Connecting, Transmitting).

3. Detailed Feature Requirements
Wi-Fi and Networking
Dual-Mode Connectivity: The device operates in two modes:

Station Mode: Connects to a user-configured Wi-Fi network.

Provisioning AP Mode (Fallback): Creates a temporary Access Point if the Station Mode connection fails.

Provisioning AP Details:

SSID is dynamically generated (e.g., Beacon-XXXX from the MAC address).

Serves a web page to configure Wi-Fi credentials (SSID/password) and a custom mDNS hostname.

Settings are saved to NVS.

Connection Logic:

Attempts to connect in Station mode for 10 tries.

On failure, falls back to Provisioning AP mode for exactly 5 minutes.

After 5 minutes, it reverts to Station mode to retry. This cycle repeats indefinitely.

mDNS Service Discovery:

In AP mode: beacon-XXXX.local.

In Station mode: The user-defined hostname (e.g., my-wspr-beacon.local).

Time Management
Adaptive NTP Synchronization:

Uses NTP for precise time.

The NTP client is adaptive: it models the ESP32's internal clock drift and triggers a new sync before the predicted time error exceeds 500ms.

Time Zone Support:

The web UI allows the user to set their local time zone.

Default time zone is GMT.

All scheduling is based on the user's selected local time.

Web Interface & Control
Navigation: All pages share a consistent navigation menu on the left side.

Configuration Page: Allows setting all operational parameters:

Beacon Identity: Callsign, Maidenhead Locator, and Power (dBm).

Master Schedule: Multiple active time windows, definable by start/stop times and days of the week.

Band Plan: A sequence of bands for rotation.

Transmission Logic: Per-band settings for:

Number of transmissions before rotating to the next band.

Number of 2-minute WSPR intervals to skip between transmissions.

Optional frequency override for each band.

Status & Statistics Page: A read-only page displaying:

Current operational status.

Total transmission minutes accumulated per band.

4. Hardware & System
MCU: ESP32.

Synthesizer: Si5351.

Partition Table: A custom partition scheme is used to provide a single, large 3MB application partition, foregoing OTA capability.

5. Development History & Known Issues
The project has undergone significant refactoring from C-style code to a C++ class-based architecture.

Extensive debugging has occurred related to ESP-IDF build system dependencies (json, mdns, driver), C++ exception handling, linker errors, and I2C driver API usage.

The si5351 driver has been successfully refactored into a C++ class.

The user has reported a UI bug where the file cabinet icon in the chat interface can cause a context reset.
