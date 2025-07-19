#pragma once

#include <cstdint>

class Si5351Intf {
public:
  virtual ~Si5351Intf() {}

  virtual void init() = 0;
  virtual void setFrequency(int channel, double freqHz) = 0;
  virtual void enableOutput(int channel, bool enable) = 0;
  virtual void reset() = 0;
  virtual void setCalibration(int32_t correction) = 0;
};