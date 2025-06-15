#include "Scheduler.h"
#include "esp_log.h"
#include <time.h>

static const char* TAG = "Scheduler";

// The scheduler will run its check once per second.
static const uint64_t SCHEDULER_INTERVAL_US = 1000 * 1000;

Scheduler::Scheduler(Si5351& si5351, Settings& settings) :
  si5351(si5351),
  settings(settings),
  timer(nullptr),
  currentBandIndex(0),
  transmissionsOnCurrentBand(0),
  skipIntervalCount(0)
{
  // The C++ initializer list handles storing the references.
}

void Scheduler::start() {
  if (timer != nullptr) {
    ESP_LOGW(TAG, "Scheduler timer already running.");
    return;
  }
  ESP_LOGI(TAG, "Starting scheduler timer.");
  const esp_timer_create_args_t timer_args = {
      .callback = &Scheduler::onTimer,
      .arg = this,
      .name = "scheduler-tick"
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, SCHEDULER_INTERVAL_US));
}

void Scheduler::stop() {
  if (timer != nullptr) {
    ESP_LOGI(TAG, "Stopping scheduler timer.");
    esp_timer_stop(timer);
    esp_timer_delete(timer);
    timer = nullptr;
  }
  // Ensure the transmitter is off when the scheduler stops.
  // si5351.outputEnable(0, 0);
}

void Scheduler::onTimer(void* arg) {
  // The static callback simply calls the instance's tick method.
  static_cast<Scheduler*>(arg)->tick();
}

void Scheduler::tick() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  // WSPR transmissions start at the beginning of an even minute (00 seconds).
  if (timeinfo.tm_sec != 0 || timeinfo.tm_min % 2 != 0) {
    return;
  }

  ESP_LOGI(TAG, "Checking schedule at %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  // 1. Check if we are within a master schedule window.
  // (Implementation of checking Settings for active time windows would go here)
  bool inActiveWindow = true; // Placeholder
  if (!inActiveWindow) {
    return;
  }

  // 2. Check the skip interval.
  // (Implementation of checking per-band skip settings would go here)
  if (skipIntervalCount > 0) {
    ESP_LOGI(TAG, "Skipping this interval (%d remaining)", skipIntervalCount);
    skipIntervalCount--;
    return;
  }

  // 3. It's time to transmit. Select the band and frequency.
  // (This is a simplified version of the band plan logic)
  ESP_LOGI(TAG, "TRANSMITTING on band index %d", currentBandIndex);

  // TODO: Get actual frequency from settings based on currentBandIndex
  // For example: long freq = settings.getBandFrequency(currentBandIndex);
  // si5351.setFrequency(0, freq);
  // si5351.outputEnable(0, 1);
  // jtencode_wspr(...)

  // 4. Update counters for the next cycle.
  transmissionsOnCurrentBand++;
  // (Implementation of checking transmissions_per_band setting would go here)
  int txPerBand = 5; // Placeholder
  if (transmissionsOnCurrentBand >= txPerBand) {
    transmissionsOnCurrentBand = 0;
    currentBandIndex++;
    // (Implementation of checking band plan size would go here)
    if (currentBandIndex >= 5) { // Placeholder for band plan size
      currentBandIndex = 0;
    }
    // (Reset skip counter for the new band based on settings)
    skipIntervalCount = 2; // Placeholder
  }
}
