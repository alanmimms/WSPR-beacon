#include "../include/Scheduler.h"
#include "../host-mock/MockTimer.h"
#include "../host-mock/Settings.h"
#include <iostream>
#include <ctime>

int main() {
    std::cout << "Simple Scheduler Test\n";
    
    MockTimer mockTimer;
    Settings settings;
    Scheduler scheduler(&mockTimer, &settings);
    
    mockTimer.logTimerActivity(true);
    
    // Set up callbacks
    int startCount = 0, endCount = 0;
    scheduler.setTransmissionStartCallback([&startCount]() { 
        std::cout << "TX START callback\n"; 
        startCount++; 
    });
    scheduler.setTransmissionEndCallback([&endCount]() { 
        std::cout << "TX END callback\n"; 
        endCount++; 
    });
    
    // Set a specific time - 12:00:00 UTC
    time_t testTime = 1609459200;  // 2021-01-01 12:00:00 UTC
    struct tm testTm;
    gmtime_r(&testTime, &testTm);
    testTm.tm_min = 0;
    testTm.tm_sec = 0;
    testTime = mktime(&testTm);
    
    mockTimer.setMockTime(testTime);
    
    std::cout << "Mock time set to: " << testTime << "\n";
    std::cout << "Timer interface returns: " << mockTimer.getCurrentTime() << "\n";
    
    scheduler.start();
    
    int nextTx = scheduler.getSecondsUntilNextTransmission();
    std::cout << "Next transmission in: " << nextTx << " seconds\n";
    
    std::cout << "Advancing time by " << nextTx << " seconds...\n";
    mockTimer.advanceTime(nextTx);
    
    std::cout << "Start count: " << startCount << ", End count: " << endCount << "\n";
    std::cout << "Transmission in progress: " << scheduler.isTransmissionInProgress() << "\n";
    
    std::cout << "\nAdvancing time by 111 seconds for transmission end...\n";
    mockTimer.advanceTime(111);
    
    std::cout << "Final counts - Start: " << startCount << ", End: " << endCount << "\n";
    std::cout << "Transmission in progress: " << scheduler.isTransmissionInProgress() << "\n";
    
    std::cout << "\nTimer log:\n";
    auto logs = mockTimer.getTimerLog();
    for (const auto& log : logs) {
        std::cout << "  " << log << "\n";
    }
    
    return 0;
}