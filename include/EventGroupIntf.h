#pragma once

#include <stdint.h>

class EventGroupIntf {
public:
  virtual ~EventGroupIntf() {}

  // Wait for any bits in bitsToWaitFor to be set, with optional timeout (ms). Returns bits that are set.
  virtual uint32_t waitBits(uint32_t bitsToWaitFor, bool clearOnExit, bool waitForAll, uint32_t timeoutMs = 0xFFFFFFFF) = 0;

  // Set bits in the event group, returns new value.
  virtual uint32_t setBits(uint32_t bitsToSet) = 0;

  // Clear bits in the event group, returns new value.
  virtual uint32_t clearBits(uint32_t bitsToClear) = 0;

  // Get current value.
  virtual uint32_t getBits() const = 0;
};