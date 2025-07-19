#pragma once

#include "Si5351Intf.h"
#include "LoggerIntf.h"

class Si5351Wrapper : public Si5351Intf {
public:
  Si5351Wrapper(LoggerIntf* logger = nullptr);
  ~Si5351Wrapper() override;

  void init() override;
  void setFrequency(int channel, double freqHz) override;
  void enableOutput(int channel, bool enable) override;
  void reset() override;
  void setCalibration(int32_t correction) override;

private:
  LoggerIntf* logger;
  void* hardware;  // Opaque pointer to the actual Si5351 hardware implementation
  bool initialized;
  double currentFrequency[3];  // Track frequencies for channels 0, 1, 2
  bool outputEnabled[3];       // Track output enable state
  
  void logRegisterWrite(int reg, int value, const char* description);
  void logFrequencyCalculation(int channel, double freqHz, const char* pllInfo);
};