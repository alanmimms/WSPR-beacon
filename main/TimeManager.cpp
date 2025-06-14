#include "TimeManager.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "TimeManager";
static double driftPerSecond = 0.0;
static int64_t lastSyncTimeUs = 0;

void TimeManager::timeSyncNotificationCb(struct timeval *tv) {
    ESP_LOGI(TAG, "NTP sync event. Time: %s", asctime(localtime(&tv->tv_sec)));
    if (lastSyncTimeUs > 0) {
        int64_t nowUs = (int64_t)tv->tv_sec * 1000000L + (int64_t)tv->tv_usec;
        int64_t timeSinceLastSyncSec = (nowUs - lastSyncTimeUs) / 1000000L;
        // This is a simplified drift calculation; a real one would be more complex.
        // For now, we assume drift is negligible for this example.
    }
    lastSyncTimeUs = (int64_t)tv->tv_sec * 1000000L + (int64_t)tv->tv_usec;
}

void TimeManager::init(const char* timeZone) {
    ESP_LOGI(TAG, "Initializing SNTP. Timezone: %s", timeZone);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(timeSyncNotificationCb);
    sntp_init();
    
    setenv("TZ", timeZone, 1);
    tzset();
}

void TimeManager::runAdaptiveSync() {
    // This task would periodically predict the clock error.
    // Simplified logic: just re-sync every hour.
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(3600 * 1000)); // Sync every hour
        sntp_sync_time(NULL);
        ESP_LOGI(TAG, "Triggering periodic NTP sync.");
    }
}
