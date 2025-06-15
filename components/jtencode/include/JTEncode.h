#ifndef JT_ENCODE_H
#define JT_ENCODE_H

#include <stdint.h>

/**
 * @brief Base class for JT/WSPR/FT8 encoders.
 */
template<uint16_t TONE_SPACING, uint16_t SYMBOL_PERIOD_MS, uint32_t DEFAULT_FREQ, int TX_BUFFER_SIZE>
class JTEncoder {
public:
  static constexpr uint16_t ToneSpacing = TONE_SPACING;
  static constexpr uint16_t SymbolPeriod = SYMBOL_PERIOD_MS;
  static constexpr int TxBufferSize = TX_BUFFER_SIZE;
  
  uint32_t txFreq;
  uint8_t symbols[TxBufferSize];

  explicit JTEncoder(uint32_t frequency = DEFAULT_FREQ) : txFreq(frequency), symbols{} {}
  virtual ~JTEncoder() = default;

  // Generic encode method for most message-based modes
  virtual void encode(const char* message) {}

  // Overload for WSPR
  virtual void encode(const char* callsign, const char *locator, int8_t powerDbm) {}

protected:
  // Encoding pipeline steps. Derived classes override what they need.
  
  // Overloaded for different packing needs.
  // Version for modes with a generic message string (FT8, JT65, etc.)
  virtual void packBits(const char* message) {}
  // Version for modes with specific fields (WSPR)
  virtual void packBits() {}

  virtual void computeFec() {}
  virtual void interleave() {}
  virtual void generateSync() {}
  virtual void convolveSymbols() {}

  uint8_t packedData[32]; // Increased size for larger modes
};

// --- Encoder Classes for Each Mode ---

class WSPREncoder : public JTEncoder<146, 683, 14097000UL + 1500, 162> {
public:
  using JTEncoder::JTEncoder;
  void encode(const char* callsign, const char* locator, int8_t powerDbm);

private:
  // This now correctly overrides the new parameter-less virtual function in the base class.
  void packBits() override;
  void convolveSymbols() override;
  void interleave() override;

  char callsign[12];
  char locator[7];
  int8_t powerDbm;
};

class FT8Encoder : public JTEncoder<625, 160, 14074000UL, 79> {
public:
  using JTEncoder::JTEncoder;
  void encode(const char* message) override;
private:
  void packBits(const char* message) override;
  void computeFec() override;
  void generateSync() override;
};

class JT65Encoder : public JTEncoder<269, 372, 14076000UL, 126> {
public:
  using JTEncoder::JTEncoder;
  void encode(const char* message) override;
private:
  void packBits(const char* message) override;
  void computeFec() override;
  void interleave() override;
  void generateSync() override;
  void convolveSymbols() override;
};

class JT9Encoder : public JTEncoder<174, 576, 14076000UL, 85> {
public:
  using JTEncoder::JTEncoder;
  void encode(const char* message) override;
private:
  // JT9 uses a slightly different pipeline structure
};

class JT4Encoder : public JTEncoder<437, 229, 14078500UL, 206> {
public:
  using JTEncoder::JTEncoder;
  void encode(const char* message) override;
private:
  // JT4 has its own unique pipeline
};


#endif // JT_ENCODE_H
