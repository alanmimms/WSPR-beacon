#pragma once

#include "Si5351Intf.h"
#include <stdio.h>

class Si5351 : public Si5351Intf {
public:
  Si5351();
  ~Si5351() override;

  void init() override;
  void setFrequency(int channel, double freqHz) override;
  void enableOutput(int channel, bool enable) override;
  void reset() override;
  void setCalibration(int32_t correction) override;
  
  // Smooth frequency transition methods for WSPR
  void setupChannelSmooth(int channel, double baseFreqHz, const double* wspr_freqs) override;
  void updateChannelFrequency(int channel, double newFreqHz) override;
  void updateChannelFrequencyMinimal(int channel, double newFreqHz) override;

  // For testing/logging purposes
  void printState();

private:
  double freq[3];
  bool outputEnabled[3];
};