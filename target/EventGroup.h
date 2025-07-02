#pragma once

#include "EventGroupIntf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

class EventGroup : public EventGroupIntf {
public:
  EventGroup();
  ~EventGroup();

  // Wait for any bits in bitsToWaitFor to be set, with optional timeout (ms). Returns bits that are set.
  uint32_t waitBits(uint32_t bitsToWaitFor, bool clearOnExit, bool waitForAll, uint32_t timeoutMs = 0xFFFFFFFF) override;

  // Set bits in the event group, returns new value.
  uint32_t setBits(uint32_t bitsToSet) override;

  // Clear bits in the event group, returns new value.
  uint32_t clearBits(uint32_t bitsToClear) override;

  // Get current value.
  uint32_t getBits() const override;

private:
  EventGroupHandle_t eventGroup_;
};