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
      .enable_internal_pullup = true
    }
  };
  ESP_LOGI(TAG, "i2c_new_master_bus");
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

  const int32_t fxtal = 25000000;
  int32_t a, b, c, x, y, z, t;

  if (fclk < 81000000) {
    a = 36; b = 0; c = 1;
    int32_t fpll = 900000000;
    x = fpll/fclk;
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
