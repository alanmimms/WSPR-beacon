#pragma once

#include "TimerIntf.h"
#include <functional>
#include <vector>
#include <memory>
#include <ctime>

class MockTimer : public TimerIntf {
public:
    struct TimerEvent {
        std::unique_ptr<Timer> timer;
        std::function<void()> callback;
        time_t triggerTime;
        bool active;
        bool oneShot;
        int id;
    };

    MockTimer();
    ~MockTimer() override;

    Timer* createOneShot(const std::function<void()>& callback) override;
    void start(Timer* timer, unsigned int timeoutMs) override;
    void stop(Timer* timer) override;
    void destroy(Timer* timer) override;
    void delayMs(int timeoutMs) override;
    void syncTime() override;
    time_t getCurrentTime() override;

    void setMockTime(time_t mockTime);
    time_t getMockTime() const;
    void advanceTime(int seconds);
    void processTimers();
    
    void setTimeAcceleration(int factor);
    int getTimeAcceleration() const;
    
    void logTimerActivity(bool enabled);
    std::vector<std::string> getTimerLog() const;
    void clearTimerLog();

private:
    time_t mockCurrentTime;
    int accelerationFactor;
    bool loggingEnabled;
    std::vector<std::string> timerLog;
    std::vector<std::unique_ptr<TimerEvent>> timers;
    
    void logActivity(const std::string& message);
    TimerEvent* findTimerEvent(Timer* timer);
};