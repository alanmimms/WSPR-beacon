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

void Si5351::setCalibration(int32_t correction) {
  printf("[Si5351HostMock] setCalibration correction=%d mPPM\n", correction);
}

void Si5351::setupChannelSmooth(int channel, double baseFreqHz, const double* wspr_freqs) {
  if (channel < 0 || channel >= 3) {
    printf("[Si5351HostMock] setupChannelSmooth invalid channel %d\n", channel);
    return;
  }
  
  freq[channel] = baseFreqHz;
  printf("[Si5351HostMock] setupChannelSmooth channel=%d baseFreq=%.6f Hz\n", channel, baseFreqHz);
  
  if (wspr_freqs) {
    printf("[Si5351HostMock] WSPR frequencies: [%.6f, %.6f, %.6f, %.6f] Hz\n", 
           wspr_freqs[0], wspr_freqs[1], wspr_freqs[2], wspr_freqs[3]);
  }
  
  printf("[Si5351HostMock] Channel %d configured for smooth WSPR frequency transitions\n", channel);
}

void Si5351::updateChannelFrequency(int channel, double newFreqHz) {
  if (channel < 0 || channel >= 3) {
    printf("[Si5351HostMock] updateChannelFrequency invalid channel %d\n", channel);
    return;
  }
  
  printf("[Si5351HostMock] Smooth frequency update: channel=%d %.6f Hz -> %.6f Hz\n", 
         channel, freq[channel], newFreqHz);
  
  freq[channel] = newFreqHz;
  printf("[Si5351HostMock] Channel %d frequency updated smoothly\n", channel);
}

void Si5351::updateChannelFrequencyMinimal(int channel, double newFreqHz) {
  if (channel < 0 || channel >= 3) {
    printf("[Si5351HostMock] updateChannelFrequencyMinimal invalid channel %d\n", channel);
    return;
  }
  
  printf("[Si5351HostMock] GLITCH-FREE frequency update: channel=%d %.6f Hz -> %.6f Hz\n", 
         channel, freq[channel], newFreqHz);
  printf("[Si5351HostMock] Disable -> Update p2 -> Phase reset -> Re-enable sequence\n");
  
  freq[channel] = newFreqHz;
  printf("[Si5351HostMock] Channel %d frequency updated glitch-free\n", channel);
}

void Si5351::printState() {
  for (int i = 0; i < 3; i++) {
    printf("[Si5351HostMock] channel %d: freq=%.6f Hz, enabled=%d\n", i, freq[i], outputEnabled[i] ? 1 : 0);
  }
}