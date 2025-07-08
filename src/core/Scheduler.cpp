#include "Scheduler.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

Scheduler::Scheduler(TimerIntf* timer, SettingsIntf* settings, LoggerIntf* logger)
    : timer(timer),
      settings(settings),
      logger(logger),
      schedulerTimer(nullptr),
      transmissionEndTimer(nullptr),
      transmissionInProgress(false),
      schedulerActive(false),
      waitingForNextOpportunity(false)
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
                logger->logError("SCHEDULER", "Failed to create periodic timer");
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
    time_t now = timer ? timer->getCurrentTime() : time(nullptr);
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
    time_t now = timer ? timer->getCurrentTime() : time(nullptr);
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

void Scheduler::checkTransmissionOpportunity() {
    if (!schedulerActive) {
        return;
    }
    
    time_t now = timer ? timer->getCurrentTime() : time(nullptr);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
    
    // Check if we're at an even minute boundary (within first 2 seconds)
    bool isTransmissionOpportunity = (tmNow.tm_min % 2 == 0) && (tmNow.tm_sec < 2);
    
    if (isTransmissionOpportunity && !transmissionInProgress && !waitingForNextOpportunity) {
        // This is a transmission opportunity - roll dice
        waitingForNextOpportunity = true; // Prevent multiple checks in same window
        
        int txPercent = settings ? settings->getInt("txPct", 0) : 0;
        int diceRoll = rand() % 100;
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
        logger->logInfo("SCHEDULER", "Transmitting for %.1f seconds", WSPR_TRANSMISSION_DURATION_SEC);
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

// Simple helper to check if band/hour is enabled
bool Scheduler::isBandEnabledForCurrentHour() const {
    // TODO: This should check band enables and hourly schedule
    // For now, always return true
    return true;
}