#pragma once

#include "GPIOIntf.h"

class GPIO : public GPIOIntf {
public:
  GPIO();
  ~GPIO() override;

  void init() override;
  void setOutput(int pin, bool value) override;
  bool getOutput(int pin) override;
  void setInput(int pin) override;
  bool readInput(int pin) override;

private:
  // Hardware-specific member variables, e.g., pin state cache if needed
};