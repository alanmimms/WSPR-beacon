This is an ESP32 based WSPR encoder with additional capabilities to do
other JTEncoder modes some day.

In the `src/jtencoder` directory I started with the GPL'ed JTEncoder
source code and rewrote most of it by [vibe coding with Google Gemini
2.5Pro](https://g.co/gemini/share/7c0f292dc869). I think the result is
simpler and easier to use and understand. I enjoyed using AI to do
this, although it was not without its frustrations.

I'm continuing my AI journey with Claude Code, which is _much_ closer
to the model I want to use in my daily work.

This beacon project can be built two ways, with the possibilty of
adding more in the future because of some interface abstraction.

1. You can build it for testing on your (Linux) host computer. This
   allows the web pages and beacon scheduler and finite state machine
   to be tested without all that icky micrcontroller overhead in the
   way.
   
2. You can build it for ESP32 using the ESP IDF (v5.4.1 or later)
   build system. I tested it with ESP32C3. This is the embedded target
   I have aimed this beacon toward initially.
   
Given the abstractions I built into the code, you can port this to
almost any microcontroller with a reasonable OS (e.g., FreeRTOS,
Zephyr, etc.) by defining some concrete implementations of the
abstract interfaces and integrating with the target's build system.

Alan Mimms, WB7NAB, 2025
