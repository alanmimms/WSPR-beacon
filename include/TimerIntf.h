#pragma once

#include <functional>

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

  // Start the timer; timeoutMs is in milliseconds
  virtual void start(Timer *timer, unsigned int timeoutMs) = 0;

  // Stop the timer if running
  virtual void stop(Timer *timer) = 0;

  // Destroy and free the timer object
  virtual void destroy(Timer *timer) = 0;

  // Optional: delay for specified milliseconds
  virtual void delayMs(int timeoutMs) = 0;

  // Optional: sync time (e.g., SNTP)
  virtual void syncTime() = 0;
};