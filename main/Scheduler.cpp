#include "Scheduler.h"
#include "ConfigManager.h"
#include "JTEncode.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

static const char* TAG = "Scheduler";

Scheduler::Scheduler() : si5351(0) {
  // The constructor of the Si5351 member object now handles all hardware initialization.
  // The correction factor is passed here.
}

void Scheduler::run() {
  BeaconConfig config;
  BeaconStats stats;
  int bandIndex = 0;
  int iterationCount = 0;
  int skipCount = 0;

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(500)); // Main loop runs twice a second

    // Load config to see if we should be running
    ConfigManager::loadConfig(config);
    if (!config.isRunning) {
      si5351.enableOutputs(0); // Use the class method to ensure transmitter is off
      continue;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Check master time schedule
    bool inActiveWindow = false;
    for (int i = 0; i < 5; ++i) {
      const auto& s = config.timeSchedules[i];
      if (s.enabled && (s.daysOfWeek & (1 << timeinfo.tm_wday))) {
	int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
	int startMinutes = s.startHour * 60 + s.startMinute;
	int endMinutes = s.endHour * 60 + s.endMinute;
	if (nowMinutes >= startMinutes && nowMinutes <= endMinutes) {
	  inActiveWindow = true;
	  break;
	}
      }
    }

    if (!inActiveWindow) continue;

    // Check if it's the start of an even minute (WSPR TX window)
    if (timeinfo.tm_sec == 0 && (timeinfo.tm_min % 2 == 0)) {
      ESP_LOGI(TAG, "WSPR window started at %d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            
      if (skipCount >= config.skipIntervals) {
	skipCount = 0; // Reset skip counter

	if(config.numBandsInPlan > 0) {
	  uint32_t freq = config.bandPlan[bandIndex].frequencyHz;
	  ESP_LOGI(TAG, "TRANSMITTING on band %d, Freq: %lu Hz", bandIndex, freq);
                    
	  // --- Call the transmission logic ---
	  transmit(freq, config.callsign, config.locator, config.powerDbm);
                    
	  ConfigManager::loadStats(stats);
	  stats.totalTxMinutes[bandIndex] += 2;
	  ConfigManager::saveStats(stats);

	  // --- Update band rotation logic ---
	  iterationCount++;
	  if(iterationCount >= config.bandPlan[bandIndex].iterations) {
	    iterationCount = 0;
	    bandIndex = (bandIndex + 1) % config.numBandsInPlan;
	    ESP_LOGI(TAG, "Rotating to next band index: %d", bandIndex);
	  }
	}
      } else {
	skipCount++;
	ESP_LOGI(TAG, "Skipping interval. Skip %d of %d", skipCount, config.skipIntervals);
      }

      // Sleep for the remainder of the 2-minute slot to prevent re-triggering
      vTaskDelay(pdMS_TO_TICKS(118 * 1000)); 
    }
  }
}

void Scheduler::transmit(uint32_t freq, const char* callsign, const char* locator, int8_t power) {
  WSPREncoder wsprEncoder(freq);
  wsprEncoder.encode(callsign, locator, power);
    
  // Use the member object to control the hardware
  si5351.enableOutputs(1 << 0); // Enable CLK0
    
  for(int i = 0; i < WSPREncoder::TxBufferSize; ++i) {
    // Calculate the exact frequency in Hz for the current symbol
    uint32_t symbolFreqHz = freq + (wsprEncoder.symbols[i] * WSPREncoder::ToneSpacing) / 100;
        
    // Use the C++ API to set the frequency for each symbol step
    si5351.setupCLK0(symbolFreqHz, Si5351::DriveStrength::MA_2);

    vTaskDelay(pdMS_TO_TICKS(WSPREncoder::SymbolPeriod));
  }
    
  si5351.enableOutputs(0); // Disable transmitter
}
