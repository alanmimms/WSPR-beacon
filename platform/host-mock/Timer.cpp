#include "Timer.h"
#include <iostream>
#include <ctime>

Timer::Timer() {}

Timer::~Timer() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Clean up any remaining timers
  for (auto& pair : timers_) {
    if (pair.second->running_) {
      pair.second->running_ = false;
      if (pair.second->thread_.joinable()) {
        pair.second->thread_.join();
      }
    }
    delete pair.second;
  }
}

TimerIntf::Timer *Timer::createOneShot(const std::function<void()> &callback) {
  auto* impl = new TimerImpl(callback, false); // One-shot
  std::lock_guard<std::mutex> lock(mutex_);
  timers_[impl] = impl;
  return impl;
}

TimerIntf::Timer *Timer::createPeriodic(const std::function<void()> &callback) {
  auto* impl = new TimerImpl(callback, true); // Periodic
  std::lock_guard<std::mutex> lock(mutex_);
  timers_[impl] = impl;
  return impl;
}

void Timer::start(TimerIntf::Timer *timer, unsigned int timeoutMs) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = timers_.find(timer);
  if (it != timers_.end()) {
    auto* impl = it->second;
    
    // Stop any existing thread
    if (impl->running_) {
      impl->running_ = false;
      if (impl->thread_.joinable()) {
        impl->thread_.join();
      }
    }
    
    // Start new timer thread
    impl->running_ = true;
    impl->thread_ = std::thread([impl, timeoutMs]() {
      if (impl->isPeriodic_) {
        // Periodic timer - keep firing until stopped
        while (impl->running_) {
          std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
          if (impl->running_) {
            impl->callback_();
          }
        }
      } else {
        // One-shot timer - fire once
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        if (impl->running_) {
          impl->callback_();
          impl->running_ = false;
        }
      }
    });
  }
}

void Timer::stop(TimerIntf::Timer *timer) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = timers_.find(timer);
  if (it != timers_.end()) {
    auto* impl = it->second;
    impl->running_ = false;
    if (impl->thread_.joinable()) {
      impl->thread_.join();
    }
  }
}

void Timer::destroy(TimerIntf::Timer *timer) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = timers_.find(timer);
  if (it != timers_.end()) {
    auto* impl = it->second;
    if (impl->running_) {
      impl->running_ = false;
      if (impl->thread_.joinable()) {
        impl->thread_.join();
      }
    }
    delete impl;
    timers_.erase(it);
  }
}

void Timer::delayMs(int timeoutMs) {
  std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
}

void Timer::syncTime() {
  // Mock implementation - just print a message
  std::cout << "Timer::syncTime() called (mock implementation)" << std::endl;
}

time_t Timer::getCurrentTime() {
  return time(nullptr);
}