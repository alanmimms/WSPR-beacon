#include "Scheduler.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"


static const char tag[] = "Scheduler";


Scheduler::Scheduler(TimerIntf* timer, SettingsIntf* settings, LoggerIntf* logger, RandomIntf* random, TimeIntf* time)
    : timer(timer),
      settings(settings),
      logger(logger),
      random(random),
      time(time),
      schedulerTimer(nullptr),
      transmissionEndTimer(nullptr),
      transmissionInProgress(false),
      schedulerActive(false),
      waitingForNextOpportunity(false),
      calibrationMode(false)
{}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::setTransmissionStartCallback(TransmissionCallback callback) {
    onTransmissionStartCallback = callback;
}

void Scheduler::setTransmissionEndCallback(TransmissionCallback callback) {
    onTransmissionEndCallback = callback;
}

void Scheduler::start() {
    if (schedulerActive) return;
    
    schedulerActive = true;
    transmissionInProgress = false;
    waitingForNextOpportunity = false;
    
    // Start a simple 1-second periodic timer
    if (!schedulerTimer) {
        schedulerTimer = timer->createPeriodic([this]() { 
            checkTransmissionOpportunity(); 
        });
        if (!schedulerTimer) {
            if (logger) {
                logger->logError(tag, "Failed to create periodic timer");
            }
            return;
        }
    }
    
    timer->start(schedulerTimer, 1000); // Check every second
}

void Scheduler::stop() {
    schedulerActive = false;
    
    if (timer) {
        if (schedulerTimer) {
            timer->stop(schedulerTimer);
            timer->destroy(schedulerTimer);
            schedulerTimer = nullptr;
        }
        if (transmissionEndTimer) {
            timer->stop(transmissionEndTimer);
            timer->destroy(transmissionEndTimer);
            transmissionEndTimer = nullptr;
        }
    }
    
    transmissionInProgress = false;
    waitingForNextOpportunity = false;
}

void Scheduler::cancelCurrentTransmission() {
    if (!transmissionInProgress) return;
    
    // Just mark transmission as not in progress
    // The task will handle calling the end callback
    transmissionInProgress = false;
}

bool Scheduler::isTransmissionInProgress() const {
    return transmissionInProgress;
}

bool Scheduler::isValidTransmissionTime() const {
    // No longer needed with task-based approach
    return false;
}

time_t Scheduler::getNextTransmissionTime() const {
    // Return next even minute boundary
    time_t now = timer ? timer->getCurrentTime() : std::time(nullptr);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
    
    int minutesToNext = (tmNow.tm_min % 2 == 0) ? 2 : 1;
    if (tmNow.tm_min % 2 == 0 && tmNow.tm_sec == 0) {
        minutesToNext = 0;  // We're exactly at an even minute
    }
    
    time_t nextTime = now + (minutesToNext * 60) - tmNow.tm_sec;
    return nextTime;
}

int Scheduler::getSecondsUntilNextTransmission() const {
    // Return seconds until next even minute (transmission opportunity)
    time_t now = timer ? timer->getCurrentTime() : std::time(nullptr);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
    
    int secondsToWait;
    if (tmNow.tm_min % 2 == 0) {
        // Already on even minute
        if (tmNow.tm_sec < 2) {
            secondsToWait = 0;  // Within first 2 seconds of even minute
        } else {
            secondsToWait = 120 - tmNow.tm_sec;  // Wait for next even minute
        }
    } else {
        // On odd minute, wait until next even minute
        secondsToWait = 60 - tmNow.tm_sec;
    }
    
    return secondsToWait;
}

int Scheduler::getSecondsUntilNextActualTransmission() const {
    if (!time || !settings) {
        // Fallback to next opportunity if we can't properly calculate
        return getSecondsUntilNextTransmission();
    }
    
    // Get transmission probability percentage
    int txPercent = settings->getInt("txPct", 0);
    if (txPercent == 0) {
        // No transmissions will occur
        return -1; // Indicate no transmission will happen
    }
    
    // Start from current time and check each transmission opportunity
    int64_t currentTime = time->getTime();
    int maxHoursToCheck = 24; // Don't look beyond 24 hours
    int opportunities = 0;
    
    for (int hoursAhead = 0; hoursAhead < maxHoursToCheck; hoursAhead++) {
        int64_t checkTime = currentTime + (hoursAhead * 3600);
        int futureHour = time->getUTCHour(checkTime);
        
        // Check if any bands are enabled for this hour
        if (hasAnyEnabledBandsForHour(futureHour)) {
            // Calculate opportunities in this hour (30 per hour, every 2 minutes)
            int startMinute = (hoursAhead == 0) ? 
                ((currentTime % 3600) / 60) : 0; // Start from current minute if current hour
            
            for (int minute = startMinute; minute < 60; minute += 2) {
                int64_t opportunityTime = checkTime - (checkTime % 3600) + (minute * 60);
                
                // Skip if this opportunity is in the past
                if (opportunityTime <= currentTime) {
                    continue;
                }
                
                opportunities++;
                
                // On average, we'll transmit every (100/txPercent) opportunities
                // This is a probabilistic estimate
                if (opportunities >= (100 / txPercent)) {
                    return (int)(opportunityTime - currentTime);
                }
            }
        }
    }
    
    // If no transmission expected in 24 hours, return -1
    return -1;
}

void Scheduler::checkTransmissionOpportunity() {
    if (!schedulerActive) {
        return;
    }
    
    time_t now = timer ? timer->getCurrentTime() : std::time(nullptr);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
    
    // Check if we're at an even minute boundary (within first 2 seconds)
    bool isTransmissionOpportunity = (tmNow.tm_min % 2 == 0) && (tmNow.tm_sec < 2);
    
    if (isTransmissionOpportunity && !transmissionInProgress && !waitingForNextOpportunity && !calibrationMode) {
        // This is a transmission opportunity - roll dice
        waitingForNextOpportunity = true; // Prevent multiple checks in same window
        
        int txPercent = settings ? settings->getInt("txPct", 0) : 0;
        int diceRoll = random ? random->randInt(100) : 0;
        bool shouldTransmit = (txPercent > 0) && (diceRoll < txPercent);
        
        // IMPORTANT: NO LOGGING IN TIMER CALLBACK - IT'S NOT SAFE!
        // The decision will be logged when startTransmission() is called
        // or by the main loop checking for skipped transmissions
        
        if (shouldTransmit) {
            startTransmission();
        }
    }
    
    // Reset opportunity flag when we're past the window
    if (waitingForNextOpportunity && tmNow.tm_sec >= 5) {
        waitingForNextOpportunity = false;
    }
}

void Scheduler::startTransmission() {
    if (!schedulerActive || transmissionInProgress) return;
    
    transmissionInProgress = true;
    
    // Call start callback
    if (onTransmissionStartCallback) {
        onTransmissionStartCallback();
    }
    
    // Schedule end of transmission
    if (!transmissionEndTimer) {
        transmissionEndTimer = timer->createOneShot([this]() { this->onTransmissionEnd(); });
    }
    
    if (logger) {
        logger->logInfo(tag, "Transmitting for %.1f seconds", WSPR_TRANSMISSION_DURATION_SEC);
    }
    
    timer->start(transmissionEndTimer, static_cast<int>(WSPR_TRANSMISSION_DURATION_SEC * 1000));
}

void Scheduler::onTransmissionEnd() {
    transmissionInProgress = false;
    
    if (onTransmissionEndCallback) {
        onTransmissionEndCallback();
    }
    
    // The periodic timer will automatically handle the next opportunity check
}

void Scheduler::setCalibrationMode(bool enabled) {
    calibrationMode = enabled;
    if (logger) {
        logger->logInfo(tag, "Calibration mode %s", enabled ? "enabled" : "disabled");
    }
}

bool Scheduler::isCalibrationMode() const {
    return calibrationMode;
}

// Check if any enabled band has a transmission scheduled for current hour
bool Scheduler::isBandEnabledForCurrentHour() const {
    if (!time) return true; // Fallback to always true if no time interface
    
    int currentHour = time->getCurrentUTCHour();
    return hasAnyEnabledBandsForHour(currentHour);
}

// Check if a specific band is enabled for a specific hour
bool Scheduler::isBandEnabledForHour(const char* band, int hour) const {
    if (!settings || !band) return false;
    
    // Check if band is enabled
    int enabled = getBandInt(band, "en", 0);
    if (enabled == 0) {
        return false;
    }
    
    // Get schedule bitmap (32-bit integer with bits set for active hours)
    uint32_t scheduleBitmap = (uint32_t)getBandInt(band, "sched", 0xFFFFFF);
    return (scheduleBitmap & (1 << hour)) != 0;
}

// Get integer property from band settings (replicates Beacon::getBandInt logic)
int Scheduler::getBandInt(const char* band, const char* property, int defaultValue) const {
    if (!settings || !band || !property) return defaultValue;
    
    char* settingsJson = settings->toJsonString();
    if (!settingsJson) return defaultValue;
    
    cJSON* root = cJSON_Parse(settingsJson);
    if (!root) {
        free(settingsJson);
        return defaultValue;
    }
    
    cJSON* bandsObj = cJSON_GetObjectItem(root, "bands");
    if (!bandsObj) {
        // Try parsing entire object as bands
        bandsObj = root;
    }
    
    cJSON* bandObj = cJSON_GetObjectItem(bandsObj, band);
    int result = defaultValue;
    if (bandObj) {
        cJSON* propObj = cJSON_GetObjectItem(bandObj, property);
        if (cJSON_IsNumber(propObj)) {
            result = (int)cJSON_GetNumberValue(propObj);
        } else if (cJSON_IsBool(propObj)) {
            result = cJSON_IsTrue(propObj) ? 1 : 0;
        }
    }
    
    cJSON_Delete(root);
    free(settingsJson);
    return result;
}

// Check if any enabled band has transmissions scheduled for a specific hour
bool Scheduler::hasAnyEnabledBandsForHour(int hour) const {
    static const char* BAND_NAMES[12] = {
        "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"
    };
    
    for (int i = 0; i < 12; i++) {
        if (isBandEnabledForHour(BAND_NAMES[i], hour)) {
            return true;
        }
    }
    return false;
}
