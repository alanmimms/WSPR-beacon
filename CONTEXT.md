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

State Machine: A custom, pointer-based state machine manages the application's operational states.

Web Interface: A web server built on esp_http_server provides a UI for configuration and status monitoring.

Web assets (HTML, CSS, JS, favicon) are stored on a dedicated SPIFFS partition.

A captive portal with a custom DNS server redirects users to the provisioning page in AP mode.

HTTP Basic Authentication protects non-provisioning pages.

A JSON API endpoint (/api/status.json) provides dynamic data to the frontend.

Component Drivers:

si5351: C++ class for the Si5351 chip.

Scheduler: Manages transmission timing and band plans.

Settings: A data-driven class that manages all configuration using string keys and dynamically allocated values, persisted to NVS.

dns-server: A custom component for the captive portal DNS.

Design Pattern: Dependency Injection
The BeaconFsm class creates and owns the single instances of Si5351 and Settings. It then passes references to these objects to other classes (Scheduler, WebServer) that need to access them.

3. Detailed Feature Requirements
Wi-Fi and Networking
Dual-Mode Connectivity: Operates in Station Mode (connecting to a configured Wi-Fi) or falls back to a Provisioning AP Mode.

Debug Mode: A compile-time flag (USE_STATIC_WIFI_CREDS) allows bypassing AP mode to connect directly to a hardcoded Wi-Fi network for easier development.

Web Interface & Control
Layout: All pages share a consistent layout with a header, footer, and side navigation menu.

Provisioning Page: (provisioning.html) Allows setting up Wi-Fi, hostname, and initial admin credentials.

Control Pages: (index.html, security.html) Are protected by authentication and allow for monitoring and further configuration.

4. Hardware & System
MCU: ESP32-C3

Synthesizer: Si5351

Partition Table: A custom partitions.csv defines partitions for the application, NVS, and a storage partition for the SPIFFS filesystem.

5. Development History & Key Decisions
The project was refactored from a complex template-based state machine (tinyfsm) to a simpler, more maintainable pointer-based state machine.

Web content was moved from being embedded in C++ code to a separate SPIFFS partition, managed by the spiffs_create_partition_image build command.

An "HTTP 431: Request Header Fields Too Large" error during authentication was resolved by increasing the Max HTTP Request Header Length in the HTTP Server component configuration via menuconfig. Previous attempts to fix this in code were incorrect as the issue was with the server's compile-time buffer allocation.

The Settings class was refactored to be fully data-driven, using an array of structs and generic getValue/setValue methods instead of hardcoded getters and setters.
