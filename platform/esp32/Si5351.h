#pragma once

#include "Si5351Intf.h"

class Si5351 : public Si5351Intf {
public:
  Si5351();
  ~Si5351() override;

  void init() override;
  void setFrequency(int channel, double freqHz) override;
  void enableOutput(int channel, bool enable) override;
  void reset() override;

private:
  // Add any ESP-IDF/I2C device handles or state needed for real hardware
};