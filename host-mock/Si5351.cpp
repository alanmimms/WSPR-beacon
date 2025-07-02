#include "Si5351.h"
#include <stdio.h>
#include <string.h>

Si5351::Si5351() {
  for (int i = 0; i < 3; i++) {
    freq[i] = 0.0;
    outputEnabled[i] = false;
  }
}

Si5351::~Si5351() {
}

void Si5351::init() {
  printf("[Si5351HostMock] init called\n");
}

void Si5351::setFrequency(int channel, double freqHz) {
  if (channel < 0 || channel >= 3) {
    printf("[Si5351HostMock] setFrequency invalid channel %d\n", channel);
    return;
  }
  freq[channel] = freqHz;
  printf("[Si5351HostMock] setFrequency channel=%d freq=%.6f Hz\n", channel, freqHz);
}

void Si5351::enableOutput(int channel, bool enable) {
  if (channel < 0 || channel >= 3) {
    printf("[Si5351HostMock] enableOutput invalid channel %d\n", channel);
    return;
  }
  outputEnabled[channel] = enable;
  printf("[Si5351HostMock] enableOutput channel=%d enable=%d\n", channel, enable ? 1 : 0);
}

void Si5351::reset() {
  printf("[Si5351HostMock] reset called\n");
  for (int i = 0; i < 3; i++) {
    freq[i] = 0.0;
    outputEnabled[i] = false;
  }
}

void Si5351::printState() {
  for (int i = 0; i < 3; i++) {
    printf("[Si5351HostMock] channel %d: freq=%.6f Hz, enabled=%d\n", i, freq[i], outputEnabled[i] ? 1 : 0);
  }
}