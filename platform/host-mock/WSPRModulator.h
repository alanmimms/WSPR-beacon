#pragma once

#include "WSPRModulatorIntf.h"
#include "TimerIntf.h"
#include <functional>

/**
 * Host-mock timer-based WSPR modulator implementation
 * 
 * Uses the TimerIntf periodic timer to achieve 683ms symbol intervals
 * for WSPR protocol compliance during testing.
 */
class WSPRModulator : public WSPRModulatorIntf {
public:
    WSPRModulator(TimerIntf* timer);
    ~WSPRModulator() override;
    
    bool startModulation(const std::function<void(int symbolIndex)>& symbolCallback, int totalSymbols) override;
    void stopModulation() override;
    bool isModulationActive() const override;
    int getCurrentSymbolIndex() const override;
    
private:
    // Timer callback function
    void onTimerCallback();
    
    // Dependencies
    TimerIntf* timer;
    TimerIntf::Timer* modulationTimer;
    
    // State
    std::function<void(int)> symbolCallback;
    int totalSymbols;
    int currentSymbolIndex;
    bool modulationActive;
};