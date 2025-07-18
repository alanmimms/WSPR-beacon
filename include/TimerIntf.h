#pragma once

#include <functional>
#include <ctime>

// Abstract timer interface and timer object for cross-platform use.

class TimerIntf {
public:
  class Timer {
  public:
    virtual ~Timer() {}
  };

  virtual ~TimerIntf() {}

  // Create a one-shot timer (fires after timeout, once)
  virtual Timer *createOneShot(const std::function<void()> &callback) = 0;
  
  // Create a periodic timer (fires repeatedly at interval)
  virtual Timer *createPeriodic(const std::function<void()> &callback) = 0;

  // Start the timer; timeoutMs is in milliseconds
  virtual void start(Timer *timer, unsigned int timeoutMs) = 0;

  // Stop the timer if running
  virtual void stop(Timer *timer) = 0;

  // Destroy and free the timer object
  virtual void destroy(Timer *timer) = 0;

  // Optional: delay for specified milliseconds
  virtual void delayMs(int timeoutMs) = 0;

  // Execute callback with precise timing - maintains intervalMs regardless of callback duration
  virtual void executeWithPreciseTiming(const std::function<void()> &callback, int intervalMs) = 0;

  // Optional: sync time (e.g., SNTP)
  virtual void syncTime() = 0;
  
  // Get current time (for testing/mocking)
  virtual time_t getCurrentTime() = 0;
};