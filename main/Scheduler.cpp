#include "Scheduler.h"
#include "Settings.h"
#include "esp_log.h"
#include "JTEncode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "si5351.h"

static const char* TAG = "Scheduler";

Scheduler::Scheduler(Si5351& si5351, gpio_num_t statusLedPin) :
  si5351(si5351),
  timer(nullptr),
  statusLedPin(statusLedPin) {
}

void Scheduler::start() {
  ESP_LOGI(TAG, "Starting scheduler...");
  const esp_timer_create_args_t timer_args = {
      .callback = &Scheduler::timerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "beacon_timer",
      .skip_unhandled_events = false
  };

  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
  
  // Perform the first transmission immediately without waiting for a timer event.
  // The timer will be armed inside the transmit() function for all subsequent runs.
  transmit();
}

void Scheduler::stop() {
  if (timer) {
    esp_timer_stop(timer);
    esp_timer_delete(timer);
    timer = nullptr;
  }
  ESP_LOGI(TAG, "Scheduler stopped.");
}

void Scheduler::timerCallback(void* arg) {
  // This static function is called by the ESP-IDF timer service.
  // It calls the instance-specific transmit method.
  Scheduler* scheduler = static_cast<Scheduler*>(arg);
  scheduler->transmit();
}

void Scheduler::transmit() {
  // --- Retrieve latest settings ---
  char callsign[MAX_CALLSIGN_LEN];
  char grid[MAX_GRID_LEN];
  getSettingString("callsign", callsign, sizeof(callsign), "N0CALL");
  getSettingString("grid", grid, sizeof(grid), "AA00aa");
  int powerDBm = getSettingInt("powerDBm", 10);

  ESP_LOGI(TAG, "TX cycle: %s, %s, %d dBm.", callsign, grid, powerDBm);
  gpio_set_level(statusLedPin, 1);
  
  // TODO: Add WSPR/JT9 signal generation logic here using jtencode and si5351
  
  vTaskDelay(pdMS_TO_TICKS(2000)); // Simulate transmission duration
  
  gpio_set_level(statusLedPin, 0);

  // --- Reschedule for the next interval ---
  int txIntervalMinutes = getSettingInt("txIntervalMinutes", 10);
  uint64_t intervalMicroseconds = (uint64_t)txIntervalMinutes * 60 * 1000 * 1000;
  
  ESP_LOGI(TAG, "Transmission finished. Scheduling next in %d minutes.", txIntervalMinutes);
  if (timer) {
    // Arm the one-shot timer for the next transmission.
    ESP_ERROR_CHECK(esp_timer_start_once(timer, intervalMicroseconds));
  }
}
