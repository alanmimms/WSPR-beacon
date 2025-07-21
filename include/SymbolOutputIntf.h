#pragma once

#include <cstdint>

/**
 * Interface for WSPR symbol visualization output
 * Abstracts the display/logging of symbol information during transmission
 */
class SymbolOutputIntf {
public:
    virtual ~SymbolOutputIntf() = default;
    
    /**
     * Start symbol stream visualization
     * @param firstSymbol The first symbol value (0-3)
     */
    virtual void startSymbolStream(int firstSymbol) = 0;
    
    /**
     * Output a single symbol during transmission
     * @param symbolIndex Index of the symbol (0-161 for WSPR)
     * @param symbolValue Symbol value (0-3, representing frequency tones)
     */
    virtual void outputSymbol(int symbolIndex, int symbolValue) = 0;
    
    /**
     * End symbol stream visualization
     */
    virtual void endSymbolStream() = 0;
    
    /**
     * Output symbol encoding information (for debugging)
     * @param symbols Array of symbol values
     * @param count Number of symbols
     */
    virtual void outputSymbolArray(const uint8_t* symbols, int count) = 0;
};