#include "Si5351.h"
#include "../../target-esp32/components/si5351/include/si5351.h"
#include <stdexcept>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

static const char *TAG = "Si5351Platform";

Si5351Wrapper::Si5351Wrapper(LoggerIntf* logger) 
  : logger(logger), hardware(nullptr), initialized(false) {
  // Initialize tracking arrays
  for (int i = 0; i < 3; i++) {
    currentFrequency[i] = 0.0;
    outputEnabled[i] = false;
  }
  
  if (logger) {
    logger->logInfo(TAG, "Si5351 platform wrapper created");
  }
}

Si5351Wrapper::~Si5351Wrapper() {
  if (hardware) {
    if (logger) {
      logger->logInfo(TAG, "Destroying Si5351 hardware instance");
    }
    delete static_cast<Si5351*>(hardware);
    hardware = nullptr;
  }
}

void Si5351Wrapper::init() {
  if (initialized) {
    if (logger) {
      logger->logWarn(TAG, "Si5351 already initialized, skipping");
    }
    return;
  }
  
  // Use Kconfig values for I2C configuration
  uint8_t i2cAddr = CONFIG_SI5351_ADDRESS;
  uint8_t sdaPin = CONFIG_I2C_MASTER_SDA;
  uint8_t sclPin = CONFIG_I2C_MASTER_SCL;
  
  if (logger) {
    logger->logInfo(TAG, "Initializing Si5351 hardware...");
    logger->logInfo(TAG, "I2C Address: 0x%02X, SDA: GPIO%d, SCL: GPIO%d", i2cAddr, sdaPin, sclPin);
  }
  
  // Create the actual hardware instance using Kconfig values
  hardware = new Si5351(i2cAddr, sdaPin, sclPin, 0); // 0 correction for now
  
  if (logger) {
    logger->logInfo(TAG, "Si5351 hardware created successfully");
    logger->logInfo(TAG, "Crystal: 25MHz, Load capacitance: 10pF");
    logger->logInfo(TAG, "All outputs initially disabled");
  }
  
  initialized = true;
  
  // Log initial state
  logRegisterWrite(3, 0xFF, "OUTPUT_ENABLE_CONTROL - All outputs disabled");
  logRegisterWrite(16, 0x80, "CLK0_CONTROL - Powered down");
  logRegisterWrite(17, 0x80, "CLK1_CONTROL - Powered down"); 
  logRegisterWrite(18, 0x80, "CLK2_CONTROL - Powered down");
  logRegisterWrite(183, 0xD0, "CRYSTAL_LOAD - 10pF load capacitance");
}

void Si5351Wrapper::setFrequency(int channel, double freqHz) {
  if (!initialized || !hardware) {
    if (logger) {
      logger->logError(TAG, "setFrequency called but Si5351 not initialized (channel %d, freq %.6f MHz)", 
                       channel, freqHz / 1000000.0);
    }
    return;
  }
  
  if (channel < 0 || channel > 2) {
    if (logger) {
      logger->logError(TAG, "Invalid channel %d (must be 0-2)", channel);
    }
    return;
  }
  
  // Store the new frequency
  double previousFreq = currentFrequency[channel];
  currentFrequency[channel] = freqHz;
  
  if (logger) {
    logger->logDebug(TAG, "Setting CLK%d frequency: %.6f MHz (was %.6f MHz)", 
                     channel, freqHz / 1000000.0, previousFreq / 1000000.0);
    
    // Log WSPR-specific information if this looks like WSPR
    if (freqHz >= 1800000 && freqHz <= 30000000) {
      const char* bandName = "Unknown";
      if (freqHz >= 1838000 && freqHz <= 1838300) bandName = "160m";
      else if (freqHz >= 3570000 && freqHz <= 3570300) bandName = "80m";
      else if (freqHz >= 7040000 && freqHz <= 7040300) bandName = "40m";
      else if (freqHz >= 10140000 && freqHz <= 10140300) bandName = "30m";
      else if (freqHz >= 14097000 && freqHz <= 14097300) bandName = "20m";
      else if (freqHz >= 18106000 && freqHz <= 18106300) bandName = "17m";
      else if (freqHz >= 21096000 && freqHz <= 21096300) bandName = "15m";
      else if (freqHz >= 24926000 && freqHz <= 24926300) bandName = "12m";
      else if (freqHz >= 28126000 && freqHz <= 28126300) bandName = "10m";
      
      logger->logDebug(TAG, "WSPR band: %s, Frequency within WSPR allocation: %s", 
                       bandName, 
                       (freqHz >= 1838000 && freqHz <= 30000000) ? "YES" : "NO");
    }
  }
  
  // Use the appropriate setup method based on channel
  Si5351* hw = static_cast<Si5351*>(hardware);
  if (channel == 0) {
    hw->setupCLK0((int32_t)freqHz, Si5351::DriveStrength::MA_8); // ~10.7 dBm
    logFrequencyCalculation(channel, freqHz, "PLL A");
  } else if (channel == 2) {
    hw->setupCLK2((int32_t)freqHz, Si5351::DriveStrength::MA_8); // ~10.7 dBm  
    logFrequencyCalculation(channel, freqHz, "PLL B");
  } else {
    if (logger) {
      logger->logWarn(TAG, "CLK1 setup not implemented in current Si5351 hardware");
    }
  }
  
  if (logger) {
    logger->logDebug(TAG, "CLK%d frequency set successfully", channel);
  }
}

void Si5351Wrapper::enableOutput(int channel, bool enable) {
  if (!initialized || !hardware) {
    if (logger) {
      logger->logError(TAG, "enableOutput called but Si5351 not initialized (channel %d, enable %s)", 
                       channel, enable ? "true" : "false");
    }
    return;
  }
  
  if (channel < 0 || channel > 2) {
    if (logger) {
      logger->logError(TAG, "Invalid channel %d (must be 0-2)", channel);
    }
    return;
  }
  
  bool previousState = outputEnabled[channel];
  outputEnabled[channel] = enable;
  
  if (logger) {
    logger->logDebug(TAG, "CLK%d output: %s -> %s (freq: %.6f MHz)", 
                     channel, 
                     previousState ? "ENABLED" : "DISABLED",
                     enable ? "ENABLED" : "DISABLED",
                     currentFrequency[channel] / 1000000.0);
  }
  
  // Calculate the enable mask for this channel
  uint8_t currentMask = 0;
  for (int i = 0; i < 3; i++) {
    if (outputEnabled[i]) {
      currentMask |= (1 << i);
    }
  }
  
  static_cast<Si5351*>(hardware)->enableOutputs(currentMask);
  
  if (logger) {
    logger->logDebug(TAG, "Output enable mask: 0x%02X (CLK0:%s CLK1:%s CLK2:%s)", 
                     currentMask,
                     (currentMask & 0x01) ? "ON" : "OFF",
                     (currentMask & 0x02) ? "ON" : "OFF", 
                     (currentMask & 0x04) ? "ON" : "OFF");
  }
  
  logRegisterWrite(3, ~currentMask, "OUTPUT_ENABLE_CONTROL");
  
  if (enable && currentFrequency[channel] > 0) {
    if (logger) {
      logger->logInfo(TAG, "RF OUTPUT: CLK%d enabled at %.6f MHz", 
                      channel, currentFrequency[channel] / 1000000.0);
    }
  } else if (!enable) {
    if (logger) {
      logger->logInfo(TAG, "RF OUTPUT: CLK%d disabled", channel);
    }
  }
}

void Si5351Wrapper::reset() {
  if (logger) {
    logger->logInfo(TAG, "Resetting Si5351...");
  }
  
  // Reset our tracking state
  for (int i = 0; i < 3; i++) {
    currentFrequency[i] = 0.0;
    outputEnabled[i] = false;
  }
  
  if (hardware) {
    delete static_cast<Si5351*>(hardware);
    hardware = nullptr;
  }
  
  initialized = false;
  
  // Re-initialize
  init();
  
  if (logger) {
    logger->logInfo(TAG, "Si5351 reset complete");
  }
}

void Si5351Wrapper::logRegisterWrite(int reg, int value, const char* description) {
  if (logger) {
    logger->logDebug(TAG, "REG[%d] = 0x%02X (%s)", reg, value, description);
  }
}

void Si5351Wrapper::logFrequencyCalculation(int channel, double freqHz, const char* pllInfo) {
  if (logger) {
    // Calculate approximate PLL frequency for logging
    double pllFreq = 900000000.0; // Typical PLL frequency for lower frequencies
    if (freqHz > 81000000) {
      pllFreq = freqHz * 6; // Rough estimate for higher frequencies
    }
    
    logger->logDebug(TAG, "CLK%d: Target=%.6f MHz, %s PLL~%.1f MHz, Drive=8mA (~10.7dBm)", 
                     channel, freqHz / 1000000.0, pllInfo, pllFreq / 1000000.0);
  }
}

void Si5351Wrapper::setCalibration(int32_t correction) {
  if (!initialized || !hardware) {
    if (logger) {
      logger->logError(TAG, "Cannot set calibration - Si5351 not initialized");
    }
    return;
  }

  if (logger) {
    logger->logInfo(TAG, "Setting Si5351 calibration correction to %d mPPM", correction);
  }

  Si5351* si5351 = static_cast<Si5351*>(hardware);
  si5351->setCorrection(correction);

  if (logger) {
    logger->logInfo(TAG, "Si5351 calibration applied successfully");
  }
}