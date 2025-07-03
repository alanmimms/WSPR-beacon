#pragma once

#include "AppContext.h"
#include "FSM.h"
#include "Scheduler.h"

class Beacon {
public:
    explicit Beacon(AppContext* ctx);
    ~Beacon();

    void run();
    void stop();

private:
    void initialize();
    void initializeHardware();
    void initializeWebServer();
    void startNetworking();
    
    void onStateChanged(FSM::NetworkState networkState, FSM::TransmissionState txState);
    void onTransmissionStart();
    void onTransmissionEnd();
    void onSettingsChanged();
    
    void startTransmission();
    void endTransmission();
    void syncTime();
    void periodicTimeSync();
    
    bool shouldConnectToWiFi() const;
    bool connectToWiFi();
    void startAccessPoint();
    
    AppContext* ctx;
    FSM fsm;
    Scheduler scheduler;
    
    bool running;
    time_t lastTimeSync;
    
    static constexpr const char* DEFAULT_SETTINGS_JSON = 
        "{"
        "\"callsign\":\"N0CALL\","
        "\"locator\":\"AA00aa\","
        "\"powerDbm\":10,"
        "\"txIntervalMinutes\":4"
        "}";
};