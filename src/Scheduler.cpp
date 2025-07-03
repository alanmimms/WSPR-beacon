#include "Scheduler.h"
#include <cstring>

Scheduler::Scheduler(TimerIntf* timer, SettingsIntf* settings)
    : timer(timer),
      settings(settings),
      transmissionStartTimer(nullptr),
      transmissionEndTimer(nullptr),
      transmissionInProgress(false),
      schedulerActive(false)
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
    scheduleNextTransmission();
}

void Scheduler::stop() {
    schedulerActive = false;
    
    if (timer) {
        if (transmissionStartTimer) {
            timer->stop(transmissionStartTimer);
            timer->destroy(transmissionStartTimer);
            transmissionStartTimer = nullptr;
        }
        if (transmissionEndTimer) {
            timer->stop(transmissionEndTimer);
            timer->destroy(transmissionEndTimer);
            transmissionEndTimer = nullptr;
        }
    }
    
    transmissionInProgress = false;
}

void Scheduler::cancelCurrentTransmission() {
    if (!transmissionInProgress) return;
    
    if (timer && transmissionEndTimer) {
        timer->stop(transmissionEndTimer);
    }
    
    onTransmissionEnd();
}

bool Scheduler::isTransmissionInProgress() const {
    return transmissionInProgress;
}

bool Scheduler::isValidTransmissionTime() const {
    time_t now = timer ? timer->getCurrentTime() : time(nullptr);
    struct tm tmNow;
    gmtime_r(&now, &tmNow);
    
    int intervalMinutes = settings ? settings->getInt("txIntervalMinutes", 4) : 4;
    
    return isEvenMinuteBoundary(now, intervalMinutes) && 
           tmNow.tm_sec >= WSPR_START_OFFSET_SEC && 
           tmNow.tm_sec < (WSPR_START_OFFSET_SEC + 5);
}

time_t Scheduler::getNextTransmissionTime() const {
    time_t now = timer ? timer->getCurrentTime() : time(nullptr);
    int intervalMinutes = settings ? settings->getInt("txIntervalMinutes", 4) : 4;
    return calculateNextEvenMinute(now, intervalMinutes);
}

int Scheduler::getSecondsUntilNextTransmission() const {
    time_t now = timer ? timer->getCurrentTime() : time(nullptr);
    time_t nextTx = getNextTransmissionTime();
    return static_cast<int>(nextTx - now) + WSPR_START_OFFSET_SEC;
}

void Scheduler::scheduleNextTransmission() {
    if (!schedulerActive || !timer) return;
    
    int secondsUntilNext = getSecondsUntilNextTransmission();
    
    if (secondsUntilNext < 5) {
        int intervalMinutes = settings ? settings->getInt("txIntervalMinutes", 4) : 4;
        secondsUntilNext += intervalMinutes * 60;
    }
    
    if (!transmissionStartTimer) {
        transmissionStartTimer = timer->createOneShot([this]() { this->onTransmissionStart(); });
    }
    
    timer->start(transmissionStartTimer, secondsUntilNext * 1000);
}

void Scheduler::onTransmissionStart() {
    if (!schedulerActive) return;
    
    transmissionInProgress = true;
    
    if (onTransmissionStartCallback) {
        onTransmissionStartCallback();
    }
    
    if (timer) {
        if (!transmissionEndTimer) {
            transmissionEndTimer = timer->createOneShot([this]() { this->onTransmissionEnd(); });
        }
        timer->start(transmissionEndTimer, static_cast<int>(WSPR_TRANSMISSION_DURATION_SEC * 1000));
    }
}

void Scheduler::onTransmissionEnd() {
    transmissionInProgress = false;
    
    if (onTransmissionEndCallback) {
        onTransmissionEndCallback();
    }
    
    if (schedulerActive) {
        scheduleNextTransmission();
    }
}

time_t Scheduler::calculateNextEvenMinute(time_t currentTime, int intervalMinutes) const {
    struct tm tmCurrent;
    gmtime_r(&currentTime, &tmCurrent);
    
    int currentMinute = tmCurrent.tm_min;
    int minutesPastInterval = currentMinute % intervalMinutes;
    int minutesToNext = intervalMinutes - minutesPastInterval;
    
    if (minutesPastInterval == 0 && tmCurrent.tm_sec < WSPR_START_OFFSET_SEC) {
        minutesToNext = 0;
    }
    
    // Calculate next time directly with seconds arithmetic to avoid mktime() UTC issues
    time_t nextTime = currentTime;
    nextTime += minutesToNext * 60;           // Add minutes
    nextTime -= (tmCurrent.tm_sec);           // Remove current seconds to align to minute boundary
    
    return nextTime;
}

bool Scheduler::isEvenMinuteBoundary(time_t time, int intervalMinutes) const {
    struct tm tmTime;
    gmtime_r(&time, &tmTime);
    return (tmTime.tm_min % intervalMinutes) == 0;
}