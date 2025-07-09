#include "WSPRModulator.h"
#include "esp_log.h"

static const char* TAG = "WSPRModulator";

WSPRModulator::WSPRModulator() 
    : taskHandle(nullptr),
      totalSymbols(0),
      currentSymbolIndex(-1),
      modulationActive(false)
{
}

WSPRModulator::~WSPRModulator() {
    stopModulation();
}

bool WSPRModulator::startModulation(const std::function<void(int symbolIndex)>& callback, int symbols) {
    if (modulationActive) {
        ESP_LOGW(TAG, "Modulation already active");
        return false;
    }
    
    symbolCallback = callback;
    totalSymbols = symbols;
    currentSymbolIndex = 0;
    modulationActive = true;
    
    // Create FreeRTOS task for precise timing
    BaseType_t result = xTaskCreate(
        modulationTask,               // Task function
        "wspr_mod",                   // Task name
        4096,                         // Stack size (4KB)
        this,                         // Parameter (pass this pointer)
        10,                           // Priority (higher than normal)
        &taskHandle                   // Task handle storage
    );
    
    if (result == pdPASS) {
        ESP_LOGI(TAG, "WSPR modulation task created (683ms symbol period)");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to create WSPR modulation task");
        modulationActive = false;
        return false;
    }
}

void WSPRModulator::stopModulation() {
    if (!modulationActive) return;
    
    modulationActive = false;
    
    // Delete the FreeRTOS task if it exists
    if (taskHandle) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
    
    currentSymbolIndex = -1;
    ESP_LOGI(TAG, "WSPR modulation stopped");
}

bool WSPRModulator::isModulationActive() const {
    return modulationActive;
}

int WSPRModulator::getCurrentSymbolIndex() const {
    return currentSymbolIndex;
}

void WSPRModulator::modulationTask(void* param) {
    WSPRModulator* modulator = static_cast<WSPRModulator*>(param);
    
    // Get the starting time for accurate periodic wakeup
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(683); // 683ms per WSPR symbol
    
    // Call callback for symbol 0 immediately
    if (modulator->symbolCallback && modulator->currentSymbolIndex == 0) {
        modulator->symbolCallback(0);
    }
    
    while (modulator->modulationActive && modulator->currentSymbolIndex < modulator->totalSymbols - 1) {
        // Wait until next symbol period
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
        
        // Move to next symbol
        modulator->currentSymbolIndex = modulator->currentSymbolIndex + 1;
        
        // Call the callback for the next symbol
        if (modulator->symbolCallback && modulator->currentSymbolIndex < modulator->totalSymbols) {
            modulator->symbolCallback(modulator->currentSymbolIndex);
        }
    }
    
    // Task cleanup - set handle to nullptr before deleting
    modulator->taskHandle = nullptr;
    vTaskDelete(NULL); // Delete this task
}