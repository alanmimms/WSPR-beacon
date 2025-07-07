#pragma once

#include "TimerIntf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <map>
#include <mutex>

class Timer : public TimerIntf {
public:
  class TimerImpl : public TimerIntf::Timer {
  public:
    TimerImpl(const std::function<void()> &callback) : callback_(callback), handle_(nullptr) {}
    
    std::function<void()> callback_;
    TimerHandle_t handle_;
  };

  Timer();
  ~Timer();

  // Create a one-shot timer (fires after timeout, once)
  TimerIntf::Timer *createOneShot(const std::function<void()> &callback) override;
  
  // Create a periodic timer (fires repeatedly at interval)
  TimerIntf::Timer *createPeriodic(const std::function<void()> &callback) override;

  // Start the timer; timeoutMs is in milliseconds
  void start(TimerIntf::Timer *timer, unsigned int timeoutMs) override;

  // Stop the timer if running
  void stop(TimerIntf::Timer *timer) override;

  // Destroy and free the timer object
  void destroy(TimerIntf::Timer *timer) override;

  // Optional: delay for specified milliseconds
  void delayMs(int timeoutMs) override;

  // Optional: sync time (e.g., SNTP)
  void syncTime() override;
  
  // Get current time (for testing/mocking)
  time_t getCurrentTime() override;

private:
  static void timerCallback(TimerHandle_t xTimer);
  
  // Simple tracking for cleanup only - no complex synchronization needed
  std::map<TimerIntf::Timer*, TimerImpl*> timers_;
};