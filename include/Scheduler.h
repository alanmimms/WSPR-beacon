#pragma once

#include "TimerIntf.h"
#include "SettingsIntf.h"
#include "LoggerIntf.h"
#include "RandomIntf.h"
#include "TimeIntf.h"
#include <ctime>
#include <functional>

class Scheduler {
public:
    using TransmissionCallback = std::function<void()>;

    explicit Scheduler(TimerIntf* timer, SettingsIntf* settings, LoggerIntf* logger = nullptr, RandomIntf* random = nullptr, TimeIntf* time = nullptr);
    ~Scheduler();

    void setTransmissionStartCallback(TransmissionCallback callback);
    void setTransmissionEndCallback(TransmissionCallback callback);

    void start();
    void stop();
    void cancelCurrentTransmission();

    bool isTransmissionInProgress() const;
    bool isValidTransmissionTime() const;
    
    void setCalibrationMode(bool enabled);
    bool isCalibrationMode() const;
    
    time_t getNextTransmissionTime() const;
    int getSecondsUntilNextTransmission() const;
    
    // Calculate when next actual transmission will occur (not just opportunity)
    // considering txPct, band schedules, and enabled bands
    int getSecondsUntilNextActualTransmission() const;

    static constexpr double WSPR_TRANSMISSION_DURATION_SEC = 110.592;
    static constexpr int WSPR_START_OFFSET_SEC = 1;  // Not used in new implementation

private:
    void checkTransmissionOpportunity();
    void startTransmission();
    void onTransmissionEnd();
    bool isBandEnabledForCurrentHour() const;
    
    // Band checking helpers
    bool isBandEnabledForHour(const char* band, int hour) const;
    int getBandInt(const char* band, const char* property, int defaultValue) const;
    bool hasAnyEnabledBandsForHour(int hour) const;

    TimerIntf* timer;
    SettingsIntf* settings;
    LoggerIntf* logger;
    RandomIntf* random;
    TimeIntf* time;
    
    TimerIntf::Timer* schedulerTimer;
    TimerIntf::Timer* transmissionEndTimer;
    
    TransmissionCallback onTransmissionStartCallback;
    TransmissionCallback onTransmissionEndCallback;
    
    bool transmissionInProgress;
    bool schedulerActive;
    bool waitingForNextOpportunity;
    bool calibrationMode;
};