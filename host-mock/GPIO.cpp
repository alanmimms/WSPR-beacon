#include "GPIO.h"
#include <cstdio>
#include <cstring>

GPIO::GPIO() {
  for (int i = 0; i < maxPins; i++) {
    outputState[i] = false;
    inputState[i] = false;
    isOutput[i] = false;
  }
}

GPIO::~GPIO() {}

void GPIO::init() {
  printf("[GPIOHostMock] init called\n");
}

void GPIO::setOutput(int pin, bool value) {
  if (pin < 0 || pin >= maxPins) return;
  isOutput[pin] = true;
  outputState[pin] = value;
  printf("[GPIOHostMock] setOutput pin=%d value=%d\n", pin, value ? 1 : 0);
}

bool GPIO::getOutput(int pin) {
  if (pin < 0 || pin >= maxPins) return false;
  printf("[GPIOHostMock] getOutput pin=%d -> %d\n", pin, outputState[pin] ? 1 : 0);
  return outputState[pin];
}

void GPIO::setInput(int pin) {
  if (pin < 0 || pin >= maxPins) return;
  isOutput[pin] = false;
  printf("[GPIOHostMock] setInput pin=%d\n", pin);
}

bool GPIO::readInput(int pin) {
  if (pin < 0 || pin >= maxPins) return false;
  printf("[GPIOHostMock] readInput pin=%d -> %d\n", pin, inputState[pin] ? 1 : 0);
  return inputState[pin];
}