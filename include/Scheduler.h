#pragma once

#include "TimerIntf.h"
#include "SettingsIntf.h"
#include "LoggerIntf.h"
#include <ctime>
#include <functional>

class Scheduler {
public:
    using TransmissionCallback = std::function<void()>;

    explicit Scheduler(TimerIntf* timer, SettingsIntf* settings, LoggerIntf* logger = nullptr);
    ~Scheduler();

    void setTransmissionStartCallback(TransmissionCallback callback);
    void setTransmissionEndCallback(TransmissionCallback callback);

    void start();
    void stop();
    void cancelCurrentTransmission();

    bool isTransmissionInProgress() const;
    bool isValidTransmissionTime() const;
    
    time_t getNextTransmissionTime() const;
    int getSecondsUntilNextTransmission() const;

    static constexpr double WSPR_TRANSMISSION_DURATION_SEC = 110.592;
    static constexpr int WSPR_START_OFFSET_SEC = 1;  // Not used in new implementation

private:
    void checkTransmissionOpportunity();
    void startTransmission();
    void onTransmissionEnd();
    bool isBandEnabledForCurrentHour() const;

    TimerIntf* timer;
    SettingsIntf* settings;
    LoggerIntf* logger;
    
    TimerIntf::Timer* schedulerTimer;
    TimerIntf::Timer* transmissionEndTimer;
    
    TransmissionCallback onTransmissionStartCallback;
    TransmissionCallback onTransmissionEndCallback;
    
    bool transmissionInProgress;
    bool schedulerActive;
    bool waitingForNextOpportunity;
};