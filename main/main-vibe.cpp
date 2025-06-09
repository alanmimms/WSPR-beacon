#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

// Assumes the Adafruit Si5351 library is available as a component
#include "Adafruit_SI5351.h"

// Your JTEncode library header
#include "JTEncode.h"

// --- Hardware Configuration ---
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_MASTER_FREQ_HZ 100000

// Global instance of the Si5351 controller
Adafruit_SI5351 si5351;

/**
 * @brief Initializes the I2C bus for communication with the Si5351.
 */
esp_err_t i2cMasterInit() {
  i2c_config_t conf;
  memset(&conf, 0, sizeof(conf));
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = I2C_SDA_PIN;
  conf.scl_io_num = I2C_SCL_PIN;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
  i2c_param_config(I2C_NUM_0, &conf);
  return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

/**
 * @brief FreeRTOS task to encode and transmit a WSPR signal.
 */
void wsprTxTask(void* pvParameters) {
  std::cout << "WSPR TX Task started." << std::endl;

  // 1. Instantiate the desired encoder.
  WSPREncoder wsprEncoder;

  // 2. Encode the message to generate the symbol array.
  std::cout << "Encoding WSPR message..." << std::endl;
  wsprEncoder.encode("K1ABC", "FN42", 37);
  std::cout << "Encoding complete. Beginning transmission." << std::endl;

  // Enable the Si5351 clock output
  si5351.enableOutputs(true);

  // 3. Loop through the symbols and set the synthesizer frequency.
  for (int i = 0; i < WSPREncoder::TxBufferSize; ++i) {
    // A. Calculate the target frequency in 1/100ths of a Hz.
    //    Base frequency is in Hz, so multiply by 100.
    //    ToneSpacing is already in 1/100ths of a Hz.
    uint64_t targetFreqCentiHz = ((uint64_t)wsprEncoder.txFreq * 100) 
                               + ((uint64_t)wsprEncoder.symbols[i] * WSPREncoder::ToneSpacing);

    // B. Set the Si5351 output frequency.
    //    Here we use clock output 0.
    si5351.set_freq(targetFreqCentiHz, SI5351_CLK0);
    
    // C. Wait for the duration of one symbol period.
    //    pdMS_TO_TICKS converts milliseconds to FreeRTOS ticks.
    vTaskDelay(pdMS_TO_TICKS(WSPREncoder::SymbolPeriod));
  }

  // Disable the Si5351 clock output after transmission
  si5351.enableOutputs(false);

  std::cout << "Transmission finished." << std::endl;
  vTaskDelete(NULL); // Clean up the task
}

/**
 * @brief Main application entry point.
 */
extern "C" void app_main(void) {
  std::cout << "JTEncode Si5351 Example" << std::endl;

  // Initialize the I2C bus
  if (i2cMasterInit() != ESP_OK) {
    std::cout << "I2C initialization failed." << std::endl;
    return;
  }
  std::cout << "I2C Initialized." << std::endl;

  // Initialize the Si5351 chip
  if (!si5351.begin(i2c_read, i2c_write)) {
    std::cout << "Si5351 not found." << std::endl;
    while(1) { vTaskDelay(1); }
  }
  std::cout << "Si5351 Initialized." << std::endl;

  // Set up the PLL and clock defaults if necessary.
  // For this example, we assume defaults are okay and just set frequency on the fly.

  // Create the FreeRTOS task that will handle the transmission.
  xTaskCreate(
    wsprTxTask,           // Task function
    "wsprTxTask",         // Task name
    4096,                 // Stack size
    NULL,                 // Task parameters
    5,                    // Task priority
    NULL                  // Task handle
  );
}
