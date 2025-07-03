#pragma once

#include "TimerIntf.h"
#include "SettingsIntf.h"
#include <ctime>
#include <functional>

class Scheduler {
public:
    using TransmissionCallback = std::function<void()>;

    explicit Scheduler(TimerIntf* timer, SettingsIntf* settings);
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
    static constexpr int WSPR_START_OFFSET_SEC = 1;

private:
    void scheduleNextTransmission();
    void onTransmissionStart();
    void onTransmissionEnd();
    
    time_t calculateNextEvenMinute(time_t currentTime, int intervalMinutes) const;
    bool isEvenMinuteBoundary(time_t time, int intervalMinutes) const;

    TimerIntf* timer;
    SettingsIntf* settings;
    
    TimerIntf::Timer* transmissionStartTimer;
    TimerIntf::Timer* transmissionEndTimer;
    
    TransmissionCallback onTransmissionStartCallback;
    TransmissionCallback onTransmissionEndCallback;
    
    bool transmissionInProgress;
    bool schedulerActive;
};