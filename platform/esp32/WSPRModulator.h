#pragma once

#include "WSPRModulatorIntf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <functional>

/**
 * ESP32 FreeRTOS-based WSPR modulator implementation
 * 
 * Uses a dedicated FreeRTOS task with precise vTaskDelayUntil timing
 * to achieve accurate 683ms symbol intervals required by WSPR protocol.
 */
class WSPRModulator : public WSPRModulatorIntf {
public:
    WSPRModulator();
    ~WSPRModulator() override;
    
    bool startModulation(const std::function<void(int symbolIndex)>& symbolCallback, int totalSymbols) override;
    void stopModulation() override;
    bool isModulationActive() const override;
    int getCurrentSymbolIndex() const override;
    
private:
    // FreeRTOS task function
    static void modulationTask(void* param);
    
    // Task state
    TaskHandle_t taskHandle;
    std::function<void(int)> symbolCallback;
    int totalSymbols;
    volatile int currentSymbolIndex;
    volatile bool modulationActive;
};