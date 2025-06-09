#ifndef JT_ENCODE_H
#define JT_ENCODE_H

#include <stdint.h>

/**
 * @brief Abstract base class for all JT/WSPR/FSQ encoding modes.
 * @tparam TONE_SPACING Tone spacing in Hz.
 * @tparam SYMBOL_PERIOD_MS Symbol period in milliseconds.
 * @tparam DEFAULT_FREQ Default transmission frequency in Hz.
 * @tparam TX_BUFFER_SIZE The size of the final symbol buffer.
 */
template<uint16_t TONE_SPACING, uint16_t SYMBOL_PERIOD_MS, uint32_t DEFAULT_FREQ, int TX_BUFFER_SIZE>
class JTEncoder {
public:
  // Mode-specific constants
  static constexpr uint16_t ToneSpacing = TONE_SPACING;
  static constexpr uint16_t SymbolPeriod = SYMBOL_PERIOD_MS;
  static constexpr int TxBufferSize = TX_BUFFER_SIZE;
  
  // Public member variables for simple access
  uint32_t txFreq;
  uint8_t symbols[TxBufferSize];

  explicit JTEncoder(uint32_t frequency = DEFAULT_FREQ) : txFreq(frequency) {}
  virtual ~JTEncoder() = default;

protected:
  // Internal buffer for the bit-packing stage
  uint8_t packedData[20];
};

// --- WSPR Encoder ---

class WSPREncoder : public JTEncoder<146, 683, 14097000UL + 1500, 162> {
public:
  using JTEncoder::JTEncoder; // Inherit constructor
  void encode(const char* callsign, const char* locator, int8_t powerDbm);

private:
  void packBits();
  void convolveSymbols();
  void interleave();

  // WSPR-specific data stored for the encoding process
  char callsign[12];
  char locator[7];
  int8_t powerDbm;
};

#endif // JT_ENCODE_H
