#include "../include/Scheduler.h"
#include "../host-mock/MockTimer.h"
#include "../host-mock/Settings.h"
#include <iostream>
#include <cassert>
#include <ctime>
#include <iomanip>

class SchedulerTestHarness {
public:
    struct BandConfig {
        const char* name;
        double frequency;
        int power;
    };
    
    static constexpr BandConfig bands[] = {
        {"20m", 14.097100, 23},
        {"40m", 7.040100, 30},
        {"80m", 3.570100, 30},
        {"160m", 1.838100, 37}
    };
    
    SchedulerTestHarness() 
        : mockTimer(),
          settings(),
          scheduler(&mockTimer, &settings),
          transmissionStartCount(0),
          transmissionEndCount(0),
          currentBandIndex(0)
    {
        mockTimer.logTimerActivity(true);
        mockTimer.setTimeAcceleration(100);
        
        scheduler.setTransmissionStartCallback([this]() { 
            this->onTransmissionStart(); 
        });
        scheduler.setTransmissionEndCallback([this]() { 
            this->onTransmissionEnd(); 
        });
    }
    
    void testBasicScheduling() {
        std::cout << "\n=== Test: Basic WSPR Scheduling ===\n";
        
        time_t testStart = 1609459200;
        struct tm testTm = {};
        testTm.tm_year = 121;  // 2021
        testTm.tm_mon = 0;     // January
        testTm.tm_mday = 1;    // 1st
        testTm.tm_hour = 12;   // 12:00:00 UTC
        testTm.tm_min = 0;
        testTm.tm_sec = 0;
        testStart = mktime(&testTm);
        
        mockTimer.setMockTime(testStart);
        
        scheduler.start();
        
        int nextTxSeconds = scheduler.getSecondsUntilNextTransmission();
        std::cout << "Next transmission in: " << nextTxSeconds << " seconds\n";
        
        mockTimer.advanceTime(nextTxSeconds);
        
        assert(transmissionStartCount == 1);
        std::cout << "✓ Transmission started as expected\n";
        
        std::cout << "Advancing time by " << static_cast<int>(Scheduler::WSPR_TRANSMISSION_DURATION_SEC) + 1 << " seconds...\n";
        mockTimer.advanceTime(static_cast<int>(Scheduler::WSPR_TRANSMISSION_DURATION_SEC) + 1);
        
        std::cout << "Transmission end count: " << transmissionEndCount << " (expected: 1)\n";
        std::cout << "Transmission in progress: " << scheduler.isTransmissionInProgress() << "\n";
        
        if (transmissionEndCount != 1) {
            printTimerLog();
        }
        
        assert(transmissionEndCount == 1);
        std::cout << "✓ Transmission ended after 110.6 seconds\n";
        
        scheduler.stop();
        printTimerLog();
    }
    
    void testWSPRTimingCompliance() {
        std::cout << "\n=== Test: WSPR Timing Compliance ===\n";
        
        time_t testTime = 1609459200;
        struct tm testTm;
        gmtime_r(&testTime, &testTm);
        testTm.tm_min = 0;  // Even minute
        testTm.tm_sec = 30; // 30 seconds past minute
        testTime = mktime(&testTm);
        
        mockTimer.setMockTime(testTime);
        
        bool isValid = scheduler.isValidTransmissionTime();
        std::cout << "Valid transmission time at :30 seconds: " << (isValid ? "true" : "false") << "\n";
        assert(!isValid);
        
        testTm.tm_sec = 1;  // 1 second past even minute (valid WSPR start)
        testTime = mktime(&testTm);
        mockTimer.setMockTime(testTime);
        
        isValid = scheduler.isValidTransmissionTime();
        std::cout << "Valid transmission time at :01 seconds: " << (isValid ? "true" : "false") << "\n";
        assert(isValid);
        
        std::cout << "✓ WSPR timing validation works correctly\n";
    }
    
    void testIntervalScheduling() {
        std::cout << "\n=== Test: Interval Scheduling ===\n";
        
        settings.setInt("txIntervalMinutes", 2);
        
        time_t testStart = 1609459200;
        struct tm testTm;
        gmtime_r(&testStart, &testTm);
        testTm.tm_min = 0;
        testTm.tm_sec = 0;
        testStart = mktime(&testTm);
        
        mockTimer.setMockTime(testStart);
        
        transmissionStartCount = 0;
        transmissionEndCount = 0;
        
        scheduler.start();
        
        for (int cycle = 0; cycle < 3; cycle++) {
            int nextTx = scheduler.getSecondsUntilNextTransmission();
            std::cout << "Cycle " << (cycle + 1) << ": Next TX in " << nextTx << " seconds\n";
            
            mockTimer.advanceTime(nextTx);
            mockTimer.advanceTime(static_cast<int>(Scheduler::WSPR_TRANSMISSION_DURATION_SEC) + 5);
        }
        
        assert(transmissionStartCount == 3);
        assert(transmissionEndCount == 3);
        std::cout << "✓ Multiple transmission cycles completed with 2-minute intervals\n";
        
        scheduler.stop();
        printTimerLog();
    }
    
    void testCancellation() {
        std::cout << "\n=== Test: Transmission Cancellation ===\n";
        
        time_t testStart = 1609459200;
        mockTimer.setMockTime(testStart);
        
        transmissionStartCount = 0;
        transmissionEndCount = 0;
        
        scheduler.start();
        
        int nextTx = scheduler.getSecondsUntilNextTransmission();
        mockTimer.advanceTime(nextTx);
        
        assert(transmissionStartCount == 1);
        std::cout << "Transmission started\n";
        
        mockTimer.advanceTime(50);
        scheduler.cancelCurrentTransmission();
        
        assert(transmissionEndCount == 1);
        std::cout << "✓ Transmission cancelled successfully\n";
        
        scheduler.stop();
    }
    
    void runAllTests() {
        std::cout << "Starting Scheduler Test Suite\n";
        std::cout << "Time acceleration: x" << mockTimer.getTimeAcceleration() << "\n";
        
        testBasicScheduling();
        testWSPRTimingCompliance();
        testIntervalScheduling();
        testCancellation();
        
        std::cout << "\n=== All Scheduler Tests Passed! ===\n";
    }

private:
    std::string formatUTCTime(time_t timestamp) {
        struct tm utcTm;
        gmtime_r(&timestamp, &utcTm);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &utcTm);
        return std::string(buffer);
    }
    
    BandConfig getCurrentBand() {
        return bands[currentBandIndex % (sizeof(bands) / sizeof(bands[0]))];
    }
    
    void rotateBand() {
        currentBandIndex = (currentBandIndex + 1) % (sizeof(bands) / sizeof(bands[0]));
    }
    
    void onTransmissionStart() {
        transmissionStartCount++;
        time_t currentTime = mockTimer.getCurrentTime();
        BandConfig band = getCurrentBand();
        
        std::cout << "[" << formatUTCTime(currentTime) << "] "
                  << "🟢 TX START #" << transmissionStartCount 
                  << " on " << band.name 
                  << " (" << std::fixed << std::setprecision(6) << band.frequency << " MHz) "
                  << "at " << band.power << "dBm"
                  << std::endl;
    }
    
    void onTransmissionEnd() {
        transmissionEndCount++;
        time_t currentTime = mockTimer.getCurrentTime();
        BandConfig band = getCurrentBand();
        
        std::cout << "[" << formatUTCTime(currentTime) << "] "
                  << "🔴 TX END   #" << transmissionEndCount 
                  << " on " << band.name 
                  << " (" << std::fixed << std::setprecision(6) << band.frequency << " MHz) "
                  << "after 110.6s"
                  << std::endl;
        
        rotateBand(); // Change band for next transmission
    }
    
    void printTimerLog() {
        auto logs = mockTimer.getTimerLog();
        if (logs.size() > 10) {
            std::cout << "Timer log (last 10 entries):\n";
            for (size_t i = logs.size() - 10; i < logs.size(); i++) {
                std::cout << "  " << logs[i] << "\n";
            }
        } else {
            std::cout << "Timer log:\n";
            for (const auto& log : logs) {
                std::cout << "  " << log << "\n";
            }
        }
        mockTimer.clearTimerLog();
    }
    
    MockTimer mockTimer;
    Settings settings;
    Scheduler scheduler;
    int transmissionStartCount;
    int transmissionEndCount;
    int currentBandIndex;
};