#include "Timer.h"
#include "esp_log.h"
#include "esp_sntp.h"

static const char* TAG = "Timer";

Timer::Timer() {}

Timer::~Timer() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& pair : timers_) {
    if (pair.second->handle_ != nullptr) {
      xTimerStop(pair.second->handle_, 0);
      xTimerDelete(pair.second->handle_, 0);
    }
    delete pair.second;
  }
}

void Timer::timerCallback(TimerHandle_t xTimer) {
  Timer* timer = static_cast<Timer*>(pvTimerGetTimerID(xTimer));
  if (timer) {
    std::lock_guard<std::mutex> lock(timer->mutex_);
    auto it = timer->handleToTimer_.find(xTimer);
    if (it != timer->handleToTimer_.end()) {
      it->second->callback_();
    }
  }
}

TimerIntf::Timer *Timer::createOneShot(const std::function<void()> &callback) {
  auto* impl = new TimerImpl(callback);
  
  impl->handle_ = xTimerCreate(
    "Timer",
    1, // Initial period (will be set when started)
    pdFALSE, // One-shot timer
    this, // Timer ID
    timerCallback
  );
  
  if (impl->handle_ != nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    timers_[impl] = impl;
    handleToTimer_[impl->handle_] = impl;
    return impl;
  } else {
    delete impl;
    return nullptr;
  }
}

void Timer::start(TimerIntf::Timer *timer, unsigned int timeoutMs) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = timers_.find(timer);
  if (it != timers_.end()) {
    auto* impl = it->second;
    if (impl->handle_ != nullptr) {
      xTimerChangePeriod(impl->handle_, pdMS_TO_TICKS(timeoutMs), 0);
      xTimerStart(impl->handle_, 0);
    }
  }
}

void Timer::stop(TimerIntf::Timer *timer) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = timers_.find(timer);
  if (it != timers_.end()) {
    auto* impl = it->second;
    if (impl->handle_ != nullptr) {
      xTimerStop(impl->handle_, 0);
    }
  }
}

void Timer::destroy(TimerIntf::Timer *timer) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = timers_.find(timer);
  if (it != timers_.end()) {
    auto* impl = it->second;
    if (impl->handle_ != nullptr) {
      xTimerStop(impl->handle_, 0);
      xTimerDelete(impl->handle_, 0);
      handleToTimer_.erase(impl->handle_);
    }
    delete impl;
    timers_.erase(it);
  }
}

void Timer::delayMs(int timeoutMs) {
  vTaskDelay(pdMS_TO_TICKS(timeoutMs));
}

void Timer::syncTime() {
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
  ESP_LOGI(TAG, "SNTP time sync initiated");
}