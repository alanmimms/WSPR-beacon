#include "../include/Scheduler.h"
#include "../include/FSM.h"
#include "../host-mock/MockTimer.h"
#include "../host-mock/Settings.h"
#include <iostream>
#include <ctime>
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>
#include "httplib.h"

using json = nlohmann::json;

class WebUITestServer {
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
    WebUITestServer() 
        : mockTimer(),
          settings(),
          scheduler(&mockTimer, &settings),
          fsm(),
          transmissionCount(0),
          currentBandIndex(0),
          running(false),
          timeThread(),
          server()
    {
        mockTimer.logTimerActivity(true);
        mockTimer.setTimeAcceleration(1); // No extra acceleration - advanceTimeLoop provides 10x
        
        // Set demo-friendly transmission interval (4 minutes for testing)
        settings.setInt("txIntervalMinutes", 4);
        
        scheduler.setTransmissionStartCallback([this]() { 
            this->onTransmissionStart(); 
        });
        scheduler.setTransmissionEndCallback([this]() { 
            this->onTransmissionEnd(); 
        });
        
        fsm.setStateChangeCallback([this](FSM::NetworkState netState, FSM::TransmissionState txState) {
            this->onStateChanged(netState, txState);
        });
        
        setupWebServer();
    }
    
    void start() {
        // Initialize FSM sequence
        fsm.transitionToApMode();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fsm.transitionToStaConnecting();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fsm.transitionToReady();
        
        // Set up initial time - 12:00:30 UTC (between transmissions) 
        time_t testTime = 1609459200 + 30;  // 2021-01-01 12:00:30 UTC
        
        mockTimer.setMockTime(testTime);
        scheduler.start();
        
        
        // Start time advancement thread
        running = true;
        timeThread = std::thread([this]() { this->advanceTimeLoop(); });
        
        std::cout << "WebUI Test Server starting at http://localhost:8080\n";
        std::cout << "Time acceleration: 10x (1s sim time every 100ms real time)\n";
        std::cout << "Press Ctrl+C to stop.\n";
        
        // Start web server (blocking)
        server.listen("0.0.0.0", 8080);
    }
    
    void stop() {
        running = false;
        if (timeThread.joinable()) {
            timeThread.join();
        }
        server.stop();
        scheduler.stop();
    }

private:
    void setupWebServer() {
        // Status endpoint with live data
        server.Get("/api/status.json", [this](const httplib::Request&, httplib::Response& res) {
            BandConfig currentBand = getCurrentBand();
            json status = {
                {"callsign", settings.getString("callsign", "N0CALL")},
                {"locator", settings.getString("locator", "AA00aa")},
                {"powerDbm", settings.getInt("powerDbm", 20)},
                {"currentBand", currentBand.name},
                {"currentFrequency", currentBand.frequency},
                {"networkState", fsm.getNetworkStateString()},
                {"transmissionState", fsm.getTransmissionStateString()},
                {"isTransmitting", fsm.isTransmissionActive()},
                {"transmissionCount", transmissionCount},
                {"currentTime", mockTimer.getCurrentTime()},
                {"lastResetTime", 1609459200},
                {"nextTransmissionIn", scheduler.getSecondsUntilNextTransmission()},
                {"hostname", "test-beacon.local"},
                {"statistics", {
                    {"totalTransmissions", transmissionCount},
                    {"totalMinutes", transmissionCount * 2},
                    {"byBand", {
                        {"20m", {{"transmissions", transmissionCount}, {"minutes", transmissionCount * 2}}}
                    }}
                }}
            };
            res.set_content(status.dump(), "application/json");
        });
        
        // Live status endpoint for real-time updates
        server.Get("/api/live-status", [this](const httplib::Request&, httplib::Response& res) {
            json liveStatus = {
                {"currentTime", mockTimer.getCurrentTime()},
                {"networkState", fsm.getNetworkStateString()},
                {"transmissionState", fsm.getTransmissionStateString()},
                {"isTransmitting", fsm.isTransmissionActive()},
                {"transmissionInProgress", scheduler.isTransmissionInProgress()},
                {"nextTransmissionIn", scheduler.getSecondsUntilNextTransmission()},
                {"transmissionCount", transmissionCount},
                {"timeAcceleration", mockTimer.getTimeAcceleration()}
            };
            res.set_content(liveStatus.dump(), "application/json");
        });
        
        // Time endpoint for clock sync
        server.Get("/api/time", [this](const httplib::Request&, httplib::Response& res) {
            json timeInfo = {
                {"unixTime", mockTimer.getCurrentTime()},
                {"synced", true},
                {"accelerated", true},
                {"acceleration", mockTimer.getTimeAcceleration()}
            };
            res.set_content(timeInfo.dump(), "application/json");
        });
        
        // Settings endpoints
        server.Get("/api/settings", [this](const httplib::Request&, httplib::Response& res) {
            json settingsJson = {
                {"callsign", settings.getString("callsign", "N0CALL")},
                {"locator", settings.getString("locator", "AA00aa")},
                {"powerDbm", settings.getInt("powerDbm", 10)},
                {"txIntervalMinutes", settings.getInt("txIntervalMinutes", 4)},
                {"hostname", "test-beacon.local"},
                {"bands", {
                    {"20m", {{"enabled", true}, {"frequency", 14.097100},
			     {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
					   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}}},
                    {"40m", {{"enabled", true}, {"frequency", 7.040100},
			     {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
					   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}}},
                    {"80m", {{"enabled", true}, {"frequency", 3.570100},
			     {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
					   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}}},
                    {"160m", {{"enabled", true}, {"frequency", 1.838100},
			      {"schedule", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
					   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}}}}
                }}
            };
            res.set_content(settingsJson.dump(), "application/json");
        });
        
        server.Post("/api/settings", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto contentType = req.get_header_value("Content-Type");
                if (contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
                    // Form-encoded: use params
                    for (auto& item : req.params) {
                        if (item.first == "callsign") {
                            settings.setString("callsign", item.second.c_str());
                        } else if (item.first == "locator") {
                            settings.setString("locator", item.second.c_str());
                        } else if (item.first == "powerDbm") {
                            settings.setInt("powerDbm", std::stoi(item.second));
                        } else if (item.first == "txIntervalMinutes") {
                            settings.setInt("txIntervalMinutes", std::stoi(item.second));
                        }
                    }
                } else {
                    // JSON
                    auto j = json::parse(req.body);
                    for (auto it = j.begin(); it != j.end(); ++it) {
                        if (it.key() == "callsign" && it.value().is_string()) {
                            settings.setString("callsign", it.value().get<std::string>().c_str());
                        } else if (it.key() == "locator" && it.value().is_string()) {
                            settings.setString("locator", it.value().get<std::string>().c_str());
                        } else if (it.key() == "powerDbm" && it.value().is_number()) {
                            settings.setInt("powerDbm", it.value().get<int>());
                        } else if (it.key() == "txIntervalMinutes" && it.value().is_number()) {
                            settings.setInt("txIntervalMinutes", it.value().get<int>());
                        }
                    }
                }
                
                std::cout << "[WEB] Settings updated: " 
                          << settings.getString("callsign") << ", " 
                          << settings.getString("locator") << ", "
                          << settings.getInt("powerDbm") << "dBm, "
                          << settings.getInt("txIntervalMinutes") << "min interval\n";
                
                res.status = 204;
                res.set_content("", "application/json");
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"Invalid settings format\"}", "application/json");
            }
        });
        
        // Serve static files from web directory  
        server.set_mount_point("/", "../../web");
    }
    
    void advanceTimeLoop() {
        while (running) {
            mockTimer.executeWithPreciseTiming([this]() {
                mockTimer.advanceTime(1); // Advance 1 second every 100ms (10x speed)
            }, 100);
        }
    }
    
    void onTransmissionStart() {
        BandConfig currentBand = getCurrentBand();
        time_t simTime = mockTimer.getCurrentTime();
        std::cout << "[" << formatUTCTime(simTime) << " SIM] Transmission START on " << currentBand.name 
                  << " (" << std::fixed << std::setprecision(6) << currentBand.frequency 
                  << " MHz) at " << currentBand.power << "dBm\n";
        if (fsm.canStartTransmission()) {
            fsm.transitionToTransmissionPending();
            fsm.transitionToTransmitting();
        }
    }
    
    void onTransmissionEnd() {
        transmissionCount++;
        BandConfig currentBand = getCurrentBand();
        time_t simTime = mockTimer.getCurrentTime();
        std::cout << "[" << formatUTCTime(simTime) << " SIM] Transmission END on " << currentBand.name 
                  << " (" << std::fixed << std::setprecision(6) << currentBand.frequency 
                  << " MHz) (total: " << transmissionCount << ")\n";
        if (fsm.isTransmissionActive()) {
            fsm.transitionToIdle();
        }
        rotateBand(); // Change band for next transmission
    }
    
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
    
    void onStateChanged(FSM::NetworkState, FSM::TransmissionState) {
        time_t simTime = mockTimer.getCurrentTime();
        std::cout << "[" << formatUTCTime(simTime) << " SIM] FSM State: " << fsm.getNetworkStateString() 
                  << " / " << fsm.getTransmissionStateString() << "\n";
    }
    
    MockTimer mockTimer;
    Settings settings;
    Scheduler scheduler;
    FSM fsm;
    int transmissionCount;
    int currentBandIndex;
    
    std::atomic<bool> running;
    std::thread timeThread;
    httplib::Server server;
};

int main() {
    WebUITestServer testServer;
    
    std::cout << "Starting WebUI Test Server Demo...\n";
    
    // Run for 30 seconds to demonstrate
    std::thread serverThread([&testServer]() {
        testServer.start();
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(300));
    
    std::cout << "Demo completed. Server would continue running.\n";
    testServer.stop();
    
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    return 0;
}
