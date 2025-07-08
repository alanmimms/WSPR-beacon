#include "WSPRModulator.h"
#include <iostream>

WSPRModulator::WSPRModulator(TimerIntf* timerIntf) 
    : timer(timerIntf),
      modulationTimer(nullptr),
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
        std::cout << "WSPRModulator: Modulation already active" << std::endl;
        return false;
    }
    
    if (!timer) {
        std::cout << "WSPRModulator: Timer interface not available" << std::endl;
        return false;
    }
    
    symbolCallback = callback;
    totalSymbols = symbols;
    currentSymbolIndex = 0;
    modulationActive = true;
    
    // Call callback for symbol 0 immediately
    if (symbolCallback) {
        symbolCallback(0);
    }
    
    // Create periodic timer for remaining symbols
    modulationTimer = timer->createPeriodic([this]() {
        this->onTimerCallback();
    });
    
    if (modulationTimer) {
        timer->start(modulationTimer, 683); // 683ms per WSPR symbol
        std::cout << "WSPRModulator: WSPR symbol timer started (683ms intervals)" << std::endl;
        return true;
    } else {
        std::cout << "WSPRModulator: Failed to create modulation timer" << std::endl;
        modulationActive = false;
        return false;
    }
}

void WSPRModulator::stopModulation() {
    if (!modulationActive) return;
    
    modulationActive = false;
    
    // Stop and destroy the modulation timer
    if (modulationTimer && timer) {
        timer->stop(modulationTimer);
        timer->destroy(modulationTimer);
        modulationTimer = nullptr;
    }
    
    currentSymbolIndex = -1;
    std::cout << "WSPRModulator: WSPR modulation stopped" << std::endl;
}

bool WSPRModulator::isModulationActive() const {
    return modulationActive;
}

int WSPRModulator::getCurrentSymbolIndex() const {
    return currentSymbolIndex;
}

void WSPRModulator::onTimerCallback() {
    if (!modulationActive) return;
    
    // Move to next symbol
    currentSymbolIndex++;
    
    // Check if we've transmitted all symbols
    if (currentSymbolIndex >= totalSymbols) {
        std::cout << "WSPRModulator: All " << totalSymbols << " WSPR symbols transmitted" << std::endl;
        return; // Let the main transmission timer handle the end
    }
    
    // Call the callback for the next symbol
    if (symbolCallback) {
        symbolCallback(currentSymbolIndex);
    }
}