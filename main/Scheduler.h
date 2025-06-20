#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "esp_timer.h"
#include "driver/gpio.h"
#include "Settings.h"

class Si5351;

class Scheduler {
public:
  Scheduler(Si5351 *si5351, Settings *settings, gpio_num_t statusLedPin);
  void start();
  void stop();

private:
  static void timerCallback(void *arg);
  void transmit();

  Si5351 *si5351;
  Settings *settings;
  esp_timer_handle_t timer;
  gpio_num_t statusLedPin;
};

#endif // SCHEDULER_H
