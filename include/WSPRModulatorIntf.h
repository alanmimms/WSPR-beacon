#pragma once

#include <functional>
#include <cstdint>

/**
 * Abstract interface for WSPR modulation implementations
 * 
 * This interface abstracts platform-specific WSPR modulation timing
 * and task management, allowing the core Beacon logic to remain
 * platform-independent.
 */
class WSPRModulatorIntf {
public:
    virtual ~WSPRModulatorIntf() = default;
    
    /**
     * Start WSPR modulation with precise 683ms symbol timing
     * 
     * @param symbolCallback Function called for each symbol (0-161)
     * @param totalSymbols Total number of symbols to transmit (162 for WSPR)
     * @return true if modulation started successfully
     */
    virtual bool startModulation(const std::function<void(int symbolIndex)>& symbolCallback, int totalSymbols) = 0;
    
    /**
     * Stop WSPR modulation and clean up resources
     */
    virtual void stopModulation() = 0;
    
    /**
     * Check if modulation is currently active
     * 
     * @return true if modulation is running
     */
    virtual bool isModulationActive() const = 0;
    
    /**
     * Get current symbol index being transmitted
     * 
     * @return Current symbol index (0-161) or -1 if not active
     */
    virtual int getCurrentSymbolIndex() const = 0;
};