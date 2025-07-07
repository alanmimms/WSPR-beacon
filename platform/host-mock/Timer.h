#pragma once

#include "TimerIntf.h"
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

class Timer : public TimerIntf {
public:
  class TimerImpl : public TimerIntf::Timer {
  public:
    TimerImpl(const std::function<void()> &callback, bool isPeriodic = false) 
      : callback_(callback), running_(false), isPeriodic_(isPeriodic) {}
    
    std::function<void()> callback_;
    std::atomic<bool> running_;
    std::thread thread_;
    bool isPeriodic_;
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
  
  // Get current time (returns system time for host-mock)
  time_t getCurrentTime() override;

private:
  std::mutex mutex_;
  std::map<TimerIntf::Timer*, TimerImpl*> timers_;
};