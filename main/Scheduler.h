#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "si5351.h"

class Scheduler {
public:
    Scheduler();
    void run();
private:
    void transmit(uint32_t freq, const char* callsign, const char* locator, int8_t power);
    Si5351 si5351;
};

#endif // SCHEDULER_H
