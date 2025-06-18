#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "esp_timer.h"
#include "driver/gpio.h"

// Forward declaration
class Si5351;

/**
 * @class Scheduler
 * @brief Manages the periodic transmission of beacon signals.
 */
class Scheduler {
public:
  /**
   * @brief Construct a new Scheduler object.
   *
   * @param si5351 A reference to the initialized Si5351 driver instance.
   * @param statusLedPin The GPIO pin number for the status LED.
   */
  Scheduler(Si5351& si5351, gpio_num_t statusLedPin);

  /**
   * @brief Starts the scheduling and performs the first transmission.
   */
  void start();

  /**
   * @brief Stops and cleans up the timer.
   */
  void stop();

private:
  /**
   * @brief The static callback function required by the ESP timer API.
   */
  static void timerCallback(void* arg);

  /**
   * @brief Performs the actual beacon transmission.
   */
  void transmit();
  
  Si5351& si5351;
  esp_timer_handle_t timer;
  gpio_num_t statusLedPin;
};

#endif // SCHEDULER_H
