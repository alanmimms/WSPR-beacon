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

  // For testing/logging purposes
  void printState();

private:
  double freq[3];
  bool outputEnabled[3];
};