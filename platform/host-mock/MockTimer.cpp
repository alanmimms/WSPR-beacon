#include "MockTimer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

class MockTimerImpl : public TimerIntf::Timer {
public:
    MockTimerImpl(int id) : id(id) {}
    int getId() const { return id; }
    
private:
    int id;
};

MockTimer::MockTimer() 
    : mockCurrentTime(time(nullptr)),
      accelerationFactor(1),
      loggingEnabled(false)
{
    logActivity("MockTimer initialized");
}

MockTimer::~MockTimer() {
    for (auto& timer : timers) {
        timer->active = false;
    }
    timers.clear();
}

TimerIntf::Timer* MockTimer::createOneShot(const std::function<void()>& callback) {
    static int timerId = 1;
    auto timerImpl = std::make_unique<MockTimerImpl>(timerId);
    auto timerPtr = timerImpl.get();
    
    auto event = std::make_unique<TimerEvent>();
    event->timer = std::move(timerImpl);
    event->callback = callback;
    event->triggerTime = 0;
    event->active = false;
    event->oneShot = true;
    event->id = timerId++;
    
    timers.push_back(std::move(event));
    
    logActivity("Created one-shot timer ID " + std::to_string(timerPtr->getId()));
    return timerPtr;
}

TimerIntf::Timer* MockTimer::createPeriodic(const std::function<void()>& callback) {
    static int timerId = 1;
    auto timerImpl = std::make_unique<MockTimerImpl>(timerId);
    auto timerPtr = timerImpl.get();
    
    auto event = std::make_unique<TimerEvent>();
    event->timer = std::move(timerImpl);
    event->callback = callback;
    event->triggerTime = 0;
    event->active = false;
    event->oneShot = false;
    event->id = timerId++;
    
    timers.push_back(std::move(event));
    
    logActivity("Created periodic timer ID " + std::to_string(timerPtr->getId()));
    return timerPtr;
}

void MockTimer::start(Timer* timer, unsigned int timeoutMs) {
    auto event = findTimerEvent(timer);
    if (!event) return;
    
    event->triggerTime = mockCurrentTime + (timeoutMs / 1000);
    event->active = true;
    
    std::ostringstream oss;
    oss << "Started timer ID " << event->id 
        << " for " << timeoutMs << "ms (trigger at T+" << (event->triggerTime - mockCurrentTime) << "s)";
    logActivity(oss.str());
}

void MockTimer::stop(Timer* timer) {
    auto event = findTimerEvent(timer);
    if (!event) return;
    
    event->active = false;
    
    std::ostringstream oss;
    oss << "Stopped timer ID " << event->id;
    logActivity(oss.str());
}

void MockTimer::destroy(Timer* timer) {
    auto it = std::find_if(timers.begin(), timers.end(),
        [timer](const std::unique_ptr<TimerEvent>& event) {
            return event->timer.get() == timer;
        });
    
    if (it != timers.end()) {
        std::ostringstream oss;
        oss << "Destroyed timer ID " << (*it)->id;
        logActivity(oss.str());
        
        timers.erase(it);
    }
}

void MockTimer::delayMs(int timeoutMs) {
    int delaySeconds = timeoutMs / 1000;
    if (delaySeconds > 0) {
        advanceTime(delaySeconds);
    }
    
    std::ostringstream oss;
    oss << "Delayed " << timeoutMs << "ms (advanced time by " << delaySeconds << "s)";
    logActivity(oss.str());
}

void MockTimer::executeWithPreciseTiming(const std::function<void()>& callback, int intervalMs) {
    time_t startTime = mockCurrentTime;
    
    // Execute the callback
    callback();
    
    // For mock timer, just advance by the interval (precise timing isn't critical for tests)
    int intervalSeconds = intervalMs / 1000;
    if (intervalSeconds > 0) {
        advanceTime(intervalSeconds);
    }
    
    std::ostringstream oss;
    oss << "executeWithPreciseTiming: " << intervalMs << "ms interval (advanced time by " << intervalSeconds << "s)";
    logActivity(oss.str());
}

void MockTimer::syncTime() {
    logActivity("Time sync requested (mock - no action)");
}

time_t MockTimer::getCurrentTime() {
    return mockCurrentTime;
}

void MockTimer::setMockTime(time_t mockTime) {
    time_t oldTime = mockCurrentTime;
    mockCurrentTime = mockTime;
    
    std::ostringstream oss;
    struct tm tmOld, tmNew;
    gmtime_r(&oldTime, &tmOld);
    gmtime_r(&mockTime, &tmNew);
    
    oss << "Mock time set: " << std::put_time(&tmOld, "%H:%M:%S") 
        << " -> " << std::put_time(&tmNew, "%H:%M:%S");
    logActivity(oss.str());
    
    processTimers();
}

time_t MockTimer::getMockTime() const {
    return mockCurrentTime;
}

void MockTimer::advanceTime(int seconds) {
    mockCurrentTime += seconds * accelerationFactor;
    
    std::ostringstream oss;
    oss << "Advanced time by " << seconds << "s";
    if (accelerationFactor > 1) {
        oss << " (x" << accelerationFactor << " = " << (seconds * accelerationFactor) << "s actual)";
    }
    logActivity(oss.str());
    
    processTimers();
}

void MockTimer::processTimers() {
    for (auto& event : timers) {
        if (event->active && mockCurrentTime >= event->triggerTime) {
            std::ostringstream oss;
            oss << "Triggering timer ID " << event->id
                << " at mock time " << mockCurrentTime;
            logActivity(oss.str());
            
            event->active = false;
            if (event->callback) {
                event->callback();
            }
        }
    }
}

void MockTimer::setTimeAcceleration(int factor) {
    accelerationFactor = std::max(1, factor);
    
    std::ostringstream oss;
    oss << "Time acceleration set to x" << accelerationFactor;
    logActivity(oss.str());
}

int MockTimer::getTimeAcceleration() const {
    return accelerationFactor;
}

void MockTimer::logTimerActivity(bool enabled) {
    loggingEnabled = enabled;
    if (enabled) {
        logActivity("Timer logging enabled");
    }
}

std::vector<std::string> MockTimer::getTimerLog() const {
    return timerLog;
}

void MockTimer::clearTimerLog() {
    timerLog.clear();
}

void MockTimer::logActivity(const std::string& message) {
    if (!loggingEnabled) return;
    
    struct tm tmNow;
    gmtime_r(&mockCurrentTime, &tmNow);
    
    std::ostringstream oss;
    oss << "[" << std::put_time(&tmNow, "%H:%M:%S") << "] " << message;
    
    timerLog.push_back(oss.str());
    
    if (timerLog.size() > 1000) {
        timerLog.erase(timerLog.begin());
    }
}

MockTimer::TimerEvent* MockTimer::findTimerEvent(Timer* timer) {
    for (auto& event : timers) {
        if (event->timer.get() == timer) {
            return event.get();
        }
    }
    return nullptr;
}
