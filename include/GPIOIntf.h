#pragma once

class GPIOIntf {
public:
  virtual ~GPIOIntf() {}

  virtual void init() = 0;
  virtual void setOutput(int pin, bool value) = 0;
  virtual bool getOutput(int pin) = 0;
  virtual void setInput(int pin) = 0;
  virtual bool readInput(int pin) = 0;
};