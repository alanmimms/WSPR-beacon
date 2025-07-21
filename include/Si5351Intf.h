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
  
  // Smooth frequency transition methods for WSPR
  virtual void setupChannelSmooth(int channel, double baseFreqHz, const double* wspr_freqs) = 0;
  virtual void updateChannelFrequency(int channel, double newFreqHz) = 0;
  virtual void updateChannelFrequencyMinimal(int channel, double newFreqHz) = 0;
};