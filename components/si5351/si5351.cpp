// vim: set ai et ts=4 sw=4:

#include "si5351.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"

// Hardware Defines - change these to match your hardware setup
#define I2C_PORT        I2C_NUM_0
#define I2C_MASTER_SDA  (gpio_num_t)21
#define I2C_MASTER_SCL  (gpio_num_t)22

// Private procedures
static uint8_t si5351Write(uint8_t reg, uint8_t data);
static void si5351WriteBulk(uint8_t baseaddr, int32_t P1, int32_t P2, int32_t P3, uint8_t divBy4, si5351RDiv_t rdiv);


// See http://www.silabs.com/Support%20Documents/TechnicalDocs/AN619.pdf
enum {
    SI5351_REGISTER_0_DEVICE_STATUS                       = 0,
    SI5351_REGISTER_3_OUTPUT_ENABLE_CONTROL               = 3,
    SI5351_REGISTER_16_CLK0_CONTROL                       = 16,
    SI5351_REGISTER_17_CLK1_CONTROL                       = 17,
    SI5351_REGISTER_18_CLK2_CONTROL                       = 18,
    SI5351_REGISTER_19_CLK3_CONTROL                       = 19,
    SI5351_REGISTER_20_CLK4_CONTROL                       = 20,
    SI5351_REGISTER_21_CLK5_CONTROL                       = 21,
    SI5351_REGISTER_22_CLK6_CONTROL                       = 22,
    SI5351_REGISTER_23_CLK7_CONTROL                       = 23,
    SI5351_REGISTER_42_MULTISYNTH0_PARAMETERS_1           = 42,
    SI5351_REGISTER_50_MULTISYNTH1_PARAMETERS_1           = 50,
    SI5351_REGISTER_58_MULTISYNTH2_PARAMETERS_1           = 58,
    SI5351_REGISTER_165_CLK0_INITIAL_PHASE_OFFSET         = 165,
    SI5351_REGISTER_166_CLK1_INITIAL_PHASE_OFFSET         = 166,
    SI5351_REGISTER_167_CLK2_INITIAL_PHASE_OFFSET         = 167,
    SI5351_REGISTER_177_PLL_RESET                         = 177,
    SI5351_REGISTER_183_CRYSTAL_INTERNAL_LOAD_CAPACITANCE = 183
};

typedef enum {
    SI5351_CRYSTAL_LOAD_6PF  = (1<<6),
    SI5351_CRYSTAL_LOAD_8PF  = (2<<6),
    SI5351_CRYSTAL_LOAD_10PF = (3<<6)
} si5351CrystalLoad_t;

static int32_t si5351Correction;

// Use modern ESP-IDF I2C driver handles
static i2c_master_bus_handle_t busHandle;
static i2c_master_dev_handle_t si5351Handle;


static void i2cInit() {
  i2c_master_bus_config_t busConfig = {
    .i2c_port = I2C_PORT,
    .sda_io_num = I2C_MASTER_SDA,
    .scl_io_num = I2C_MASTER_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .trans_queue_depth = 0,
    .flags = {
        .enable_internal_pullup = true
    }
  };

  ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &busHandle));

  i2c_device_config_t devConfig = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = SI5351_ADDRESS,
    .scl_speed_hz = 100000,
  };
  
  ESP_ERROR_CHECK(i2c_master_bus_add_device(busHandle, &devConfig, &si5351Handle));
}


void si5351_Init(int32_t correction) {
    i2cInit();

    si5351Correction = correction;

    si5351Write(SI5351_REGISTER_3_OUTPUT_ENABLE_CONTROL, 0xFF);

    si5351Write(SI5351_REGISTER_16_CLK0_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_17_CLK1_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_18_CLK2_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_19_CLK3_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_20_CLK4_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_21_CLK5_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_22_CLK6_CONTROL, 0x80);
    si5351Write(SI5351_REGISTER_23_CLK7_CONTROL, 0x80);

    si5351CrystalLoad_t crystalLoad = SI5351_CRYSTAL_LOAD_10PF;
    si5351Write(SI5351_REGISTER_183_CRYSTAL_INTERNAL_LOAD_CAPACITANCE, crystalLoad);
}

void si5351_SetupPLL(si5351PLL_t pll, si5351PLLConfig_t* conf) {
    int32_t P1, P2, P3;
    int32_t mult = conf->mult;
    int32_t num = conf->num;
    int32_t denom = conf->denom;

    if (denom == 0) return; // Avoid division by zero
    P1 = 128 * mult + (128 * num)/denom - 512;
    P2 = (128 * num) % denom;
    P3 = denom;

    uint8_t baseaddr = (pll == SI5351_PLL_A ? 26 : 34);
    si5351WriteBulk(baseaddr, P1, P2, P3, 0, si5351RDiv_t::SI5351_R_DIV_1);

    si5351Write(SI5351_REGISTER_177_PLL_RESET, (1<<7) | (1<<5) );
}

int si5351_SetupOutput(uint8_t output, si5351PLL_t pllSource, si5351DriveStrength_t driveStrength, si5351OutputConfig_t* conf, uint8_t phaseOffset) {
    int32_t div = conf->div;
    int32_t num = conf->num;
    int32_t denom = conf->denom;
    uint8_t divBy4 = 0;
    int32_t P1, P2, P3;

    if (output > 2) {
        return 1;
    }

    if ((!conf->allowIntegerMode) && ((div < 8) || ((div == 8) && (num == 0)))) {
        return 2;
    }

    if (div == 4) {
        P1 = 0;
        P2 = 0;
        P3 = 1;
        divBy4 = 0x3;
    } else {
        if (denom == 0) return 3; // Avoid division by zero
        P1 = 128 * div + ((128 * num)/denom) - 512;
        P2 = (128 * num) % denom;
        P3 = denom;
    }

    uint8_t baseaddr = 0;
    uint8_t phaseOffsetRegister = 0;
    uint8_t clkControlRegister = 0;
    switch (output) {
    case 0:
        baseaddr = SI5351_REGISTER_42_MULTISYNTH0_PARAMETERS_1;
        phaseOffsetRegister = SI5351_REGISTER_165_CLK0_INITIAL_PHASE_OFFSET;
        clkControlRegister = SI5351_REGISTER_16_CLK0_CONTROL;
        break;
    case 1:
        baseaddr = SI5351_REGISTER_50_MULTISYNTH1_PARAMETERS_1;
        phaseOffsetRegister = SI5351_REGISTER_166_CLK1_INITIAL_PHASE_OFFSET;
        clkControlRegister = SI5351_REGISTER_17_CLK1_CONTROL;
        break;
    case 2:
        baseaddr = SI5351_REGISTER_58_MULTISYNTH2_PARAMETERS_1;
        phaseOffsetRegister = SI5351_REGISTER_167_CLK2_INITIAL_PHASE_OFFSET;
        clkControlRegister = SI5351_REGISTER_18_CLK2_CONTROL;
        break;
    }

    uint8_t clkControl = 0x0C | driveStrength;
    if (pllSource == SI5351_PLL_B) {
        clkControl |= (1 << 5);
    }

    if ((conf->allowIntegerMode) && ((num == 0)||(div == 4))) {
        clkControl |= (1 << 6);
    }

    si5351Write(clkControlRegister, clkControl);
    si5351WriteBulk(baseaddr, P1, P2, P3, divBy4, conf->rdiv);
    si5351Write(phaseOffsetRegister, (phaseOffset & 0x7F));

    return 0;
}

void si5351_Calc(int32_t Fclk, si5351PLLConfig_t* pll_conf, si5351OutputConfig_t* out_conf) {
    if (Fclk < 8000) Fclk = 8000;
    else if (Fclk > 160000000) Fclk = 160000000;

    out_conf->allowIntegerMode = 1;

    if (Fclk < 1000000) {
        Fclk *= 64;
        out_conf->rdiv = SI5351_R_DIV_64;
    } else {
        out_conf->rdiv = SI5351_R_DIV_1;
    }

    Fclk = Fclk - (int32_t)((((double)Fclk)/100000000.0)*((double)si5351Correction));

    const int32_t Fxtal = 25000000;
    int32_t a, b, c, x, y, z, t;

    if (Fclk < 81000000) {
        a = 36;
        b = 0;
        c = 1;
        int32_t Fpll = 900000000;
        x = Fpll/Fclk;
        t = (Fclk >> 20) + 1;
        y = (Fpll % Fclk) / t;
        z = Fclk / t;
    } else {
        if(Fclk >= 150000000) x = 4;
        else if (Fclk >= 100000000) x = 6;
        else x = 8;
        y = 0;
        z = 1;
        
        int32_t numerator = x*Fclk;
        a = numerator/Fxtal;
        t = (Fxtal >> 20) + 1;
        b = (numerator % Fxtal) / t;
        c = Fxtal / t;
    }

    pll_conf->mult = a;
    pll_conf->num = b;
    pll_conf->denom = c;
    out_conf->div = x;
    out_conf->num = y;
    out_conf->denom = z;
}

void si5351_CalcIQ(int32_t Fclk, si5351PLLConfig_t* pll_conf, si5351OutputConfig_t* out_conf) {
    const int32_t Fxtal = 25000000;
    int32_t Fpll;

    if (Fclk < 1400000) Fclk = 1400000;
    else if (Fclk > 100000000) Fclk = 100000000;

    Fclk = Fclk - ((Fclk/1000000)*si5351Correction)/100;

    out_conf->allowIntegerMode = 0;
    out_conf->rdiv = si5351RDiv_t::SI5351_R_DIV_1;

    if (Fclk < 4900000) {
        out_conf->div = 127;
    } else if(Fclk < 8000000) {
        out_conf->div = 625000000 / Fclk;
    } else {
        out_conf->div = 900000000 / Fclk;
    }
    out_conf->num = 0;
    out_conf->denom = 1;

    Fpll = Fclk * out_conf->div;
    pll_conf->mult = Fpll / Fxtal;
    pll_conf->num = (Fpll % Fxtal) / 24;
    pll_conf->denom = Fxtal / 24;
}

void si5351_SetupCLK0(int32_t Fclk, si5351DriveStrength_t driveStrength) {
	si5351PLLConfig_t pll_conf;
	si5351OutputConfig_t out_conf;

	si5351_Calc(Fclk, &pll_conf, &out_conf);
	si5351_SetupPLL(SI5351_PLL_A, &pll_conf);
	si5351_SetupOutput(0, SI5351_PLL_A, driveStrength, &out_conf, 0);
}

void si5351_SetupCLK2(int32_t Fclk, si5351DriveStrength_t driveStrength) {
	si5351PLLConfig_t pll_conf;
	si5351OutputConfig_t out_conf;

	si5351_Calc(Fclk, &pll_conf, &out_conf);
	si5351_SetupPLL(SI5351_PLL_B, &pll_conf);
	si5351_SetupOutput(2, SI5351_PLL_B, driveStrength, &out_conf, 0);
}

void si5351_EnableOutputs(uint8_t enabled) {
    si5351Write(SI5351_REGISTER_3_OUTPUT_ENABLE_CONTROL, ~enabled);
}

static uint8_t si5351Write(uint8_t reg, uint8_t data)
{
  uint8_t write_buf[2] = {reg, data};
  return i2c_master_transmit(si5351Handle, write_buf, sizeof(write_buf), -1) == ESP_OK;
}

static void si5351WriteBulk(uint8_t baseaddr, int32_t P1, int32_t P2, int32_t P3, uint8_t divBy4, si5351RDiv_t rdiv) {
    si5351Write(baseaddr,   (P3 >> 8) & 0xFF);
    si5351Write(baseaddr+1, P3 & 0xFF);
    si5351Write(baseaddr+2, ((P1 >> 16) & 0x3) | ((divBy4 & 0x3) << 2) | (((uint8_t)rdiv & 0x7) << 4));
    si5351Write(baseaddr+3, (P1 >> 8) & 0xFF);
    si5351Write(baseaddr+4, P1 & 0xFF);
    si5351Write(baseaddr+5, ((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0xF));
    si5351Write(baseaddr+6, (P2 >> 8) & 0xFF);
    si5351Write(baseaddr+7, P2 & 0xFF);
}
