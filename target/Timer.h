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

private:
  static void timerCallback(TimerHandle_t xTimer);
  
  std::mutex mutex_;
  std::map<TimerHandle_t, TimerImpl*> handleToTimer_;
  std::map<TimerIntf::Timer*, TimerImpl*> timers_;
};