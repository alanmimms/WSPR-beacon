// vim: set ai et ts=4 sw=4:

#include "si5351.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

// --- Private Register Definitions ---
enum {
  SI5351_REG_OUTPUT_ENABLE_CONTROL = 3,
  SI5351_REG_CLK0_CONTROL = 16,
  SI5351_REG_CLK1_CONTROL = 17,
  SI5351_REG_CLK2_CONTROL = 18,
  SI5351_REG_MS0_PARAMS_1 = 42,
  SI5351_REG_MS1_PARAMS_1 = 50,
  SI5351_REG_MS2_PARAMS_1 = 58,
  SI5351_REG_CLK0_PHASE_OFFSET = 165,
  SI5351_REG_CLK1_PHASE_OFFSET = 166,
  SI5351_REG_CLK2_PHASE_OFFSET = 167,
  SI5351_REG_PLL_RESET = 177,
  SI5351_REG_CRYSTAL_LOAD = 183
};

enum si5351CrystalLoad {
  CRYSTAL_LOAD_6PF  = (1<<6),
  CRYSTAL_LOAD_8PF  = (2<<6),
  CRYSTAL_LOAD_10PF = (3<<6)
};

static const char *TAG = "Si5351";

// --- Constructor / Destructor ---

Si5351::Si5351(uint8_t i2cAddr, uint8_t sdaPin, uint8_t sclPin, int32_t correctionVal) {
  correction = correctionVal;
  busHandle = nullptr;
  devHandle = nullptr;
  currentBaseFreq = 0;
  // Initialize configs to zero
  currentPLLConfig = {0, 0, 0};
  currentOutputConfig = {false, 0, 0, 0, RDiv::DIV_1};
  i2cInit(i2cAddr, sdaPin, sclPin);

  write(SI5351_REG_OUTPUT_ENABLE_CONTROL, 0xFF); // Disable all outputs
  write(SI5351_REG_CLK0_CONTROL, 0x80);
  write(SI5351_REG_CLK1_CONTROL, 0x80);
  write(SI5351_REG_CLK2_CONTROL, 0x80);

  write(SI5351_REG_CRYSTAL_LOAD, CRYSTAL_LOAD_10PF);
}

Si5351::~Si5351() {
  if (devHandle) {
    i2c_master_bus_rm_device((i2c_master_dev_handle_t)devHandle);
  }
  if (busHandle) {
    i2c_del_master_bus((i2c_master_bus_handle_t)busHandle);
  }
}

// --- Private I2C Methods ---

void Si5351::i2cInit(uint8_t i2cAddr, uint8_t sdaPin, uint8_t sclPin) {
  i2c_master_bus_config_t busConfig = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = (gpio_num_t)sdaPin,
    .scl_io_num = (gpio_num_t)sclPin,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .trans_queue_depth = 0,
    .flags = {
      .enable_internal_pullup = true,
      .allow_pd = false
    }
  };
  ESP_LOGI(TAG, "i2c_new_master_bus i2cAddr=%02X, sdaPin=%u, sclPin=%u", (int) i2cAddr, sdaPin, sclPin);
  ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, (i2c_master_bus_handle_t*)&busHandle));

  i2c_device_config_t devConfig = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = i2cAddr,
    .scl_speed_hz = 100000,
    .scl_wait_us = 0,
    .flags = 0,
  };
  ESP_LOGI(TAG, "i2c_master_bus_add_device");
  ESP_ERROR_CHECK(i2c_master_bus_add_device((i2c_master_bus_handle_t)busHandle, &devConfig,
					    (i2c_master_dev_handle_t*)&devHandle));
  ESP_LOGI(TAG, "done");
}

uint8_t Si5351::write(uint8_t reg, uint8_t data) {
  uint8_t writeBuf[2] = {reg, data};
  return i2c_master_transmit((i2c_master_dev_handle_t)devHandle, writeBuf, sizeof(writeBuf), -1) == ESP_OK;
}

void Si5351::writeBulk(uint8_t baseaddr, int32_t p1, int32_t p2, int32_t p3, uint8_t divBy4, RDiv rdiv) {
  write(baseaddr,   (p3 >> 8) & 0xFF);
  write(baseaddr+1, p3 & 0xFF);
  write(baseaddr+2, ((p1 >> 16) & 0x3) | ((divBy4 & 0x3) << 2) | (((uint8_t)rdiv & 0x7) << 4));
  write(baseaddr+3, (p1 >> 8) & 0xFF);
  write(baseaddr+4, p1 & 0xFF);
  write(baseaddr+5, ((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0xF));
  write(baseaddr+6, (p2 >> 8) & 0xFF);
  write(baseaddr+7, p2 & 0xFF);
}

void Si5351::writeFractionalOnly(uint8_t baseaddr, int32_t p2, int32_t p3) {
  // Only write the fractional part registers (baseaddr+5, +6, +7)
  // This minimizes I2C traffic and reduces glitches
  write(baseaddr+5, ((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0xF));
  write(baseaddr+6, (p2 >> 8) & 0xFF);
  write(baseaddr+7, p2 & 0xFF);
}

void Si5351::writeP2Only(uint8_t baseaddr, int32_t p2) {
  // For WSPR tones, only p2 changes, so write only registers +6 and +7
  // Use burst write to minimize glitches - single I2C transaction for both bytes
  uint8_t writeBuf[3] = {
    (uint8_t)(baseaddr + 6),  // Start register address
    (uint8_t)((p2 >> 8) & 0xFF),  // Register +6 value
    (uint8_t)(p2 & 0xFF)          // Register +7 value  
  };
  i2c_master_transmit((i2c_master_dev_handle_t)devHandle, writeBuf, sizeof(writeBuf), -1);
}

void Si5351::writeP2OnlyGlitchFree(uint8_t baseaddr, int32_t p2, uint8_t clk_num) {
  // TRULY minimal update: ONLY write the 2 changing registers
  // No output disable/enable, no phase resets, no other register touches
  // This eliminates ALL possible implicit PLL reset triggers
  
  uint8_t writeBuf[3] = {
    (uint8_t)(baseaddr + 6),        // Start register address (reg 48 for CLK0)
    (uint8_t)((p2 >> 8) & 0xFF),    // Register +6 value (p2 middle byte)
    (uint8_t)(p2 & 0xFF)            // Register +7 value (p2 low byte)
  };
  i2c_master_transmit((i2c_master_dev_handle_t)devHandle, writeBuf, sizeof(writeBuf), -1);
  
  // That's it! No other register writes that could trigger PLL resets
}

// --- Public API Implementation ---

void Si5351::setupPLL(PLL pll, const PLLConfig& conf) {
  int32_t p1, p2, p3;
  if (conf.denom == 0) return;
  p1 = 128 * conf.mult + (128 * conf.num) / conf.denom - 512;
  p2 = (128 * conf.num) % conf.denom;
  p3 = conf.denom;
  uint8_t baseaddr = (pll == PLL::A ? 26 : 34);
  writeBulk(baseaddr, p1, p2, p3, 0, RDiv::DIV_1);
  write(SI5351_REG_PLL_RESET, (1<<7) | (1<<5));
}

int Si5351::setupOutput(uint8_t output, PLL pllSource, DriveStrength driveStrength, const OutputConfig& conf, uint8_t phaseOffset) {
  int32_t div = conf.div;
  int32_t num = conf.num;
  int32_t denom = conf.denom;
  uint8_t divBy4 = 0;
  int32_t p1, p2, p3;

  if (output > 2) return 1;

  if ((!conf.allowIntegerMode) && ((div < 8) || ((div == 8) && (num == 0)))) return 2;

  if (div == 4) {
    p1 = 0; p2 = 0; p3 = 1; divBy4 = 0x3;
  } else {
    if (denom == 0) return 3;
    p1 = 128 * div + ((128 * num)/denom) - 512;
    p2 = (128 * num) % denom;
    p3 = denom;
  }

  uint8_t baseaddr = 0, ctrlReg = 0;
  switch (output) {
  case 0: baseaddr = SI5351_REG_MS0_PARAMS_1; ctrlReg = SI5351_REG_CLK0_CONTROL; break;
  case 1: baseaddr = SI5351_REG_MS1_PARAMS_1; ctrlReg = SI5351_REG_CLK1_CONTROL; break;
  case 2: baseaddr = SI5351_REG_MS2_PARAMS_1; ctrlReg = SI5351_REG_CLK2_CONTROL; break;
  }

  uint8_t clkControl = 0x0C | (uint8_t)driveStrength;
  if (pllSource == PLL::B) clkControl |= (1 << 5);
  if ((conf.allowIntegerMode) && ((num == 0)||(div == 4))) clkControl |= (1 << 6);

  write(ctrlReg, clkControl);
  writeBulk(baseaddr, p1, p2, p3, divBy4, conf.rdiv);
  // Phase offset is not implemented in this simplified driver version
    
  return 0;
}

void Si5351::calc(int32_t fclk, PLLConfig& pllConf, OutputConfig& outConf) {
  if (fclk < 8000) fclk = 8000;
  else if (fclk > 160000000) fclk = 160000000;

  outConf.allowIntegerMode = true;

  if (fclk < 1000000) {
    fclk *= 64;
    outConf.rdiv = RDiv::DIV_64;
  } else {
    outConf.rdiv = RDiv::DIV_1;
  }

  fclk = fclk - (int32_t)((((double)fclk)/100000000.0)*((double)this->correction));

  const int32_t fxtal = CONFIG_SI5351_CRYSTAL_FREQ;
  int32_t a, b, c, x, y, z, t;

  if (fclk < 81000000) {
    // For low frequencies, target PLL around 600-900 MHz
    // Choose a PLL multiplier that gives us a good divisor
    int32_t target_pll = 600000000;  // Start with 600 MHz target
    a = target_pll / fxtal;          // Initial multiplier
    
    // Find the best integer divisor
    int32_t fpll = a * fxtal;
    x = fpll / fclk;
    
    // If divisor is too large, increase PLL frequency
    while (x > 900 && a < 90) {  // Si5351 max multisynth divisor is 900
      a++;
      fpll = a * fxtal;
      x = fpll / fclk;
    }
    
    // Use fractional if needed for better accuracy
    b = 0; c = 1;
    t = (fclk >> 20) + 1;
    y = (fpll % fclk) / t;
    z = fclk / t;
  } else {
    if(fclk >= 150000000) x = 4;
    else if (fclk >= 100000000) x = 6;
    else x = 8;
    y = 0; z = 1;
        
    int32_t numerator = x * fclk;
    a = numerator / fxtal;
    t = (fxtal >> 20) + 1;
    b = (numerator % fxtal) / t;
    c = fxtal / t;
  }
  pllConf.mult = a; pllConf.num = b; pllConf.denom = c;
  outConf.div = x; outConf.num = y; outConf.denom = z;
}

void Si5351::calcIQ(int32_t fclk, PLLConfig& pllConf, OutputConfig& outConf) {
  // ... (Implementation from si5351_CalcIQ, using this->correction) ...
}

void Si5351::setupCLK0(int32_t fclk, DriveStrength driveStrength) {
  PLLConfig pllConf;
  OutputConfig outConf;
  calc(fclk, pllConf, outConf);
  
#if 0
  // Debug logging
  ESP_LOGI(TAG, "setupCLK0: Target=%ld Hz, Crystal=%d Hz", (long)fclk, CONFIG_SI5351_CRYSTAL_FREQ);
  ESP_LOGI(TAG, "PLL: mult=%ld, num=%ld, denom=%ld", (long)pllConf.mult, (long)pllConf.num, (long)pllConf.denom);
  ESP_LOGI(TAG, "Output: div=%ld, num=%ld, denom=%ld", (long)outConf.div, (long)outConf.num, (long)outConf.denom);

  // Calculate actual frequency
  int64_t pll_freq = ((int64_t)CONFIG_SI5351_CRYSTAL_FREQ * pllConf.mult) + 
                     ((int64_t)CONFIG_SI5351_CRYSTAL_FREQ * pllConf.num / pllConf.denom);
  int64_t actual_freq = pll_freq / outConf.div;
  ESP_LOGI(TAG, "Calculated: PLL=%lld Hz, Output=%lld Hz", (long long)pll_freq, (long long)actual_freq);
#endif
  
  setupPLL(PLL::A, pllConf);
  setupOutput(0, PLL::A, driveStrength, outConf, 0);
}

void Si5351::setupCLK2(int32_t fclk, DriveStrength driveStrength) {
  PLLConfig pllConf;
  OutputConfig outConf;
  calc(fclk, pllConf, outConf);
  setupPLL(PLL::B, pllConf);
  setupOutput(2, PLL::B, driveStrength, outConf, 0);
}

void Si5351::enableOutputs(uint8_t enabled) {
  write(SI5351_REG_OUTPUT_ENABLE_CONTROL, ~enabled);
}

void Si5351::setCorrection(int32_t correctionPPM) {
  correction = correctionPPM;
}

// --- Smooth Frequency Transition Methods ---

void Si5351::setupCLK0Smooth(int32_t baseFreq, const int32_t* wspr_freqs, DriveStrength driveStrength) {
  // Calculate PLL configuration that can accommodate all 4 WSPR frequencies
  // WSPR uses 4 frequencies spaced 12000/8192 = ~1.46 Hz apart
  // We need fractional synthesis to enable small frequency steps
  
  currentBaseFreq = baseFreq;
  
  // Apply frequency correction
  int32_t correctedFreq = baseFreq - (int32_t)((((double)baseFreq)/100000000.0)*((double)this->correction));
  
  const int32_t fxtal = CONFIG_SI5351_CRYSTAL_FREQ;
  
  // For WSPR, use a fixed PLL frequency that allows fractional tuning
  // Target PLL around 600-800 MHz for good fractional resolution
  int32_t target_pll = 700000000;  // 700 MHz PLL target
  int32_t a = target_pll / fxtal;  // PLL multiplier
  
  // Ensure PLL multiplier is in valid range (15-90)
  if (a < 15) a = 15;
  if (a > 90) a = 90;
  
  int32_t fpll = a * fxtal;  // Actual PLL frequency
  
  // Calculate MultiSynth divider for base frequency
  int32_t div_int = fpll / correctedFreq;
  
  // Use fractional synthesis for fine frequency control
  // Calculate fractional part with high resolution
  int64_t remainder = (int64_t)fpll - ((int64_t)div_int * correctedFreq);
  int32_t denom = 1048575;  // Max denominator for best resolution
  int32_t num = (int32_t)((remainder * denom) / correctedFreq);
  
  // Store the configuration
  currentPLLConfig.mult = a;
  currentPLLConfig.num = 0;  // Integer PLL for stability
  currentPLLConfig.denom = 1;
  
  currentOutputConfig.allowIntegerMode = false;  // Use fractional mode
  currentOutputConfig.div = div_int;
  currentOutputConfig.num = num;
  currentOutputConfig.denom = denom;
  currentOutputConfig.rdiv = RDiv::DIV_1;
  
  ESP_LOGI(TAG, "WSPR Smooth Setup: Base=%ld Hz, PLL=%ld MHz, Div=%ld.%ld/%ld", 
           (long)baseFreq, (long)(fpll/1000000), (long)div_int, (long)num, (long)denom);
  
  // Setup PLL A and CLK0 with calculated values
  setupPLL(PLL::A, currentPLLConfig);
  setupOutput(0, PLL::A, driveStrength, currentOutputConfig, 0);
}

void Si5351::updateCLK0Frequency(int32_t newFreq) {
  if (currentBaseFreq == 0) {
    ESP_LOGW(TAG, "updateCLK0Frequency called before setupCLK0Smooth");
    return;
  }
  
  // Apply frequency correction
  int32_t correctedFreq = newFreq - (int32_t)((((double)newFreq)/100000000.0)*((double)this->correction));
  
  const int32_t fxtal = CONFIG_SI5351_CRYSTAL_FREQ;
  int32_t fpll = currentPLLConfig.mult * fxtal;  // PLL frequency is fixed
  
  // Calculate new MultiSynth divider for the new frequency
  int32_t div_int = fpll / correctedFreq;
  
  // Calculate fractional part with high resolution
  int64_t remainder = (int64_t)fpll - ((int64_t)div_int * correctedFreq);
  int32_t num = (int32_t)((remainder * currentOutputConfig.denom) / correctedFreq);
  
  // Update only the MultiSynth registers, not the PLL
  OutputConfig newConfig = currentOutputConfig;
  newConfig.div = div_int;
  newConfig.num = num;
  
  // Write only the MultiSynth parameters (register 42-49 for CLK0)
  int32_t p1, p2, p3;
  if (newConfig.denom == 0) return;
  
  p1 = 128 * newConfig.div + ((128 * newConfig.num)/newConfig.denom) - 512;
  p2 = (128 * newConfig.num) % newConfig.denom;
  p3 = newConfig.denom;
  
  // Write MultiSynth registers for CLK0 (baseaddr = 42)
  writeBulk(SI5351_REG_MS0_PARAMS_1, p1, p2, p3, 0, newConfig.rdiv);
  
  // Update stored config
  currentOutputConfig = newConfig;
}

void Si5351::updateCLK0FrequencyMinimal(int32_t newFreq) {
  if (currentBaseFreq == 0) {
    ESP_LOGW(TAG, "updateCLK0FrequencyMinimal called before setupCLK0Smooth");
    return;
  }
  
  // Apply frequency correction
  int32_t correctedFreq = newFreq - (int32_t)((((double)newFreq)/100000000.0)*((double)this->correction));
  
  const int32_t fxtal = CONFIG_SI5351_CRYSTAL_FREQ;
  int32_t fpll = currentPLLConfig.mult * fxtal;  // PLL frequency is fixed
  
  // Calculate new MultiSynth divider for the new frequency
  int32_t div_int = fpll / correctedFreq;
  
  // For WSPR frequency steps (~1.46 Hz), the integer divider should stay the same
  // Only the fractional part should change
  if (div_int != currentOutputConfig.div) {
    ESP_LOGW(TAG, "Integer divider changed (%ld -> %ld), frequency step too large for minimal update", 
             (long)currentOutputConfig.div, (long)div_int);
    // Fall back to full update
    updateCLK0Frequency(newFreq);
    return;
  }
  
  // Calculate only the new fractional part
  int64_t remainder = (int64_t)fpll - ((int64_t)div_int * correctedFreq);
  int32_t num = (int32_t)((remainder * currentOutputConfig.denom) / correctedFreq);
  
  // Calculate parameters for p2 registers only
  int32_t p2 = (128 * num) % currentOutputConfig.denom;
  
  // Write p2 registers with glitch-free method (disable output during update)
  // This prevents any glitches from propagating to the output
  writeP2OnlyGlitchFree(SI5351_REG_MS0_PARAMS_1, p2, 0);  // CLK0 = clk_num 0
  
  // Update stored config
  currentOutputConfig.num = num;
  
  ESP_LOGV(TAG, "Glitch-free frequency update: %ld Hz, p2=%ld (disable-update-enable sequence)", 
           (long)newFreq, (long)p2);
}

void Si5351::setupWSPROutputs(int32_t baseFreq, DriveStrength driveStrength) {
  // Setup 4 different outputs (CLK0, CLK1, CLK2 + one more) for WSPR tones
  // This avoids ANY register writes during transmission - just output enable switching!
  
  const double tone_spacing = 1.46484375; // WSPR tone spacing in Hz
  
  ESP_LOGI(TAG, "Setting up WSPR outputs: CLK0-2 for tones 0-2, baseFreq=%ld Hz", (long)baseFreq);
  
  // Setup PLL A for all WSPR frequencies (use middle frequency for best accuracy)
  int32_t middle_freq = baseFreq + (int32_t)(1.5 * tone_spacing);
  PLLConfig pllConf;
  OutputConfig outConf;
  calc(middle_freq, pllConf, outConf);
  setupPLL(PLL::A, pllConf);
  
  // Setup each output for a specific WSPR tone
  for (int tone = 0; tone < 3; tone++) {
    int32_t tone_freq = baseFreq + (int32_t)(tone * tone_spacing);
    OutputConfig toneConf;
    calc(tone_freq, pllConf, toneConf); // Use same PLL config
    setupOutput(tone, PLL::A, driveStrength, toneConf, 0);
    
    ESP_LOGI(TAG, "CLK%d configured for WSPR tone %d: %ld Hz", tone, tone, (long)tone_freq);
  }
  
  // Disable all outputs initially
  enableOutputs(0x00);
  
  ESP_LOGI(TAG, "WSPR outputs configured - no register writes needed during transmission!");
}

void Si5351::selectWSPRTone(uint8_t tone) {
  // Switch WSPR tone by enabling different output - ZERO MultiSynth register writes!
  if (tone > 2) {
    ESP_LOGW(TAG, "Invalid WSPR tone %d, using tone 0", tone);
    tone = 0;
  }
  
  // Enable only the selected tone output
  uint8_t enable_mask = 1 << tone;
  enableOutputs(enable_mask);
  
  ESP_LOGV(TAG, "WSPR tone %d selected (CLK%d enabled)", tone, tone);
}
