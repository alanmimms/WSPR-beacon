#include "Timer.h"
#include "esp_log.h"
#include "esp_sntp.h"

static const char* TAG = "Timer";

Timer::Timer() {}

Timer::~Timer() {
  // Simple cleanup without complex locking
  for (auto& pair : timers_) {
    if (pair.second->handle_ != nullptr) {
      xTimerStop(pair.second->handle_, 0);
      xTimerDelete(pair.second->handle_, 0);
    }
    delete pair.second;
  }
}

void Timer::timerCallback(TimerHandle_t xTimer) {
  // Simplified callback - get the TimerImpl directly from timer ID
  TimerImpl* impl = static_cast<TimerImpl*>(pvTimerGetTimerID(xTimer));
  if (impl && impl->callback_) {
    impl->callback_();
  } else {
    ESP_LOGW(TAG, "Timer callback: impl=%p, callback=%p", impl, impl ? (void*)&impl->callback_ : nullptr);
  }
}

TimerIntf::Timer *Timer::createOneShot(const std::function<void()> &callback) {
  auto* impl = new TimerImpl(callback);
  
  impl->handle_ = xTimerCreate(
    "OneShotTimer",
    1, // Initial period (will be set when started)
    pdFALSE, // One-shot timer
    impl, // Timer ID - pass TimerImpl directly
    timerCallback
  );
  
  if (impl->handle_ != nullptr) {
    // Simplified tracking without mutex - just keep reference for cleanup
    timers_[impl] = impl;
    return impl;
  } else {
    delete impl;
    return nullptr;
  }
}

TimerIntf::Timer *Timer::createPeriodic(const std::function<void()> &callback) {
  auto* impl = new TimerImpl(callback);
  
  impl->handle_ = xTimerCreate(
    "PeriodicTimer",
    1, // Initial period (will be set when started)
    pdTRUE, // Periodic timer
    impl, // Timer ID - pass TimerImpl directly
    timerCallback
  );
  
  if (impl->handle_ != nullptr) {
    // Simplified tracking without mutex - just keep reference for cleanup
    timers_[impl] = impl;
    return impl;
  } else {
    ESP_LOGE(TAG, "xTimerCreate failed for periodic timer");
    delete impl;
    return nullptr;
  }
}

void Timer::start(TimerIntf::Timer *timer, unsigned int timeoutMs) {
  auto* impl = static_cast<TimerImpl*>(timer);
  if (impl && impl->handle_ != nullptr) {
    xTimerChangePeriod(impl->handle_, pdMS_TO_TICKS(timeoutMs), 0);
    xTimerStart(impl->handle_, 0);
  } else {
    ESP_LOGE(TAG, "Timer start failed: impl=%p, handle=%p", impl, impl ? impl->handle_ : nullptr);
  }
}

void Timer::stop(TimerIntf::Timer *timer) {
  auto* impl = static_cast<TimerImpl*>(timer);
  if (impl && impl->handle_ != nullptr) {
    xTimerStop(impl->handle_, 0);
  }
}

void Timer::destroy(TimerIntf::Timer *timer) {
  auto* impl = static_cast<TimerImpl*>(timer);
  if (impl) {
    if (impl->handle_ != nullptr) {
      xTimerStop(impl->handle_, 0);
      xTimerDelete(impl->handle_, 0);
    }
    // Remove from tracking map
    timers_.erase(timer);
    delete impl;
  }
}

void Timer::delayMs(int timeoutMs) {
  vTaskDelay(pdMS_TO_TICKS(timeoutMs));
}

void Timer::syncTime() {
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();
  // Avoid logging in timer context - ESP_LOGI(TAG, "SNTP time sync initiated");
}

time_t Timer::getCurrentTime() {
  return time(nullptr);
}