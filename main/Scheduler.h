#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "si5351.h" // Include the C++ class header

class Scheduler {
public:
  Scheduler();
  void run();
private:
  void transmit(uint32_t freq, const char* callsign, const char* locator, int8_t power);
    
  // The Scheduler now owns an instance of the Si5351 class.
  Si5351 si5351; 
};

#endif // SCHEDULER_H
