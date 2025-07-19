#ifndef SI5351_H
#define SI5351_H

#include <cstdint>
#include "sdkconfig.h"

class Si5351 {
public:
  // --- Public Nested Types for clarity ---
  enum class PLL {
    A,
    B
  };

  enum class RDiv {
    DIV_1   = 0,
    DIV_2   = 1,
    DIV_4   = 2,
    DIV_8   = 3,
    DIV_16  = 4,
    DIV_32  = 5,
    DIV_64  = 6,
    DIV_128 = 7,
  };

  enum class DriveStrength {
    MA_2 = 0x00, // ~2.2 dBm
    MA_4 = 0x01, // ~7.5 dBm
    MA_6 = 0x02, // ~9.5 dBm
    MA_8 = 0x03, // ~10.7 dBm
  };

  struct PLLConfig {
    int32_t mult;
    int32_t num;
    int32_t denom;
  };

  struct OutputConfig {
    bool allowIntegerMode;
    int32_t div;
    int32_t num;
    int32_t denom;
    RDiv rdiv;
  };

  // --- Constructor & Destructor ---
  explicit Si5351(uint8_t i2cAddr,
		  uint8_t sdaPin,
		  uint8_t sclPin,
		  int32_t correction = 0);
  ~Si5351();

  // Prevent copying
  Si5351(const Si5351&) = delete;
  Si5351& operator=(const Si5351&) = delete;

  // --- Public API ---
  void setupCLK0(int32_t fclk, DriveStrength driveStrength);
  void setupCLK2(int32_t fclk, DriveStrength driveStrength);
  void enableOutputs(uint8_t enabled);
  void calc(int32_t fclk, PLLConfig& pllConf, OutputConfig& outConf);
  void calcIQ(int32_t fclk, PLLConfig& pllConf, OutputConfig& outConf);
  void setupPLL(PLL pll, const PLLConfig& conf);
  int setupOutput(uint8_t output, PLL pllSource, DriveStrength driveStrength, const OutputConfig& conf, uint8_t phaseOffset);
  void setCorrection(int32_t correctionPPM);

private:
  // --- Private Methods ---
  void i2cInit(uint8_t i2cAddr, uint8_t sdaPin, uint8_t sclPin);
  uint8_t write(uint8_t reg, uint8_t data);
  void writeBulk(uint8_t baseaddr, int32_t p1, int32_t p2, int32_t p3, uint8_t divBy4, RDiv rdiv);

  // --- Private Member Variables ---
  int32_t correction;
  void* busHandle; // Use void* to hide C-style handles from the header
  void* devHandle;
};

#endif // SI5351_H
