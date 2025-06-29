#pragma once

#include <stdint.h>
#include <functional>

/**
 * Abstract interface for timers.
 * Allows scheduling one-shot and periodic timers with callback.
 * Host: can use std::thread + sleep or POSIX timer.
 * Target: wraps esp_timer or FreeRTOS timers.
 */
class TimerIntf {
public:
  virtual ~TimerIntf() {}

  // Start a one-shot timer. Fires once after delayMs, calls cb(arg).
  virtual void startOneShot(uint32_t delayMs, std::function<void(void *)> cb, void *arg) = 0;

  // Start a periodic timer. Calls cb(arg) every periodMs.
  virtual void startPeriodic(uint32_t periodMs, std::function<void(void *)> cb, void *arg) = 0;

  // Stop the timer if running.
  virtual void stop() = 0;

  // Returns true if the timer is running.
  virtual bool isActive() const = 0;
};