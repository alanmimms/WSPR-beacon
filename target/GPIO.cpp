#include "GPIO.h"
#include <driver/gpio.h>

GPIO::GPIO() {}

GPIO::~GPIO() {}

void GPIO::init() {
  // Hardware setup if needed
}

void GPIO::setOutput(int pin, bool value) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
}

bool GPIO::getOutput(int pin) {
  // ESP-IDF does not cache output, but gpio_get_level can read last set value
  return gpio_get_level((gpio_num_t)pin) != 0;
}

void GPIO::setInput(int pin) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
}

bool GPIO::readInput(int pin) {
  return gpio_get_level((gpio_num_t)pin) != 0;
}