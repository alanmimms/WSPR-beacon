#pragma once

#include "AppContext.h"
#include "FSM.h"
#include "Scheduler.h"
#include "JTEncode.h"
#include <ctime>

class Beacon {
public:
    enum class BandSelectionMode {
        SEQUENTIAL,      // Single band, no rotation
        ROUND_ROBIN,     // Cycle through bands in order
        RANDOM_EXHAUSTIVE // Random selection until all used
    };

    explicit Beacon(AppContext* ctx);
    ~Beacon();

    void run();
    void stop();

private:
    
    void onStateChanged(FSM::NetworkState networkState, FSM::TransmissionState txState);
    void onTransmissionStart();
    void onTransmissionEnd();
    void onSettingsChanged();
    
    void startTransmission();
    void endTransmission();
    void syncTime();
    void periodicTimeSync();
    
    // WSPR modulation methods
    void startWSPRModulation();
    void stopWSPRModulation();
    void modulateSymbol(int symbolIndex);
    
    bool shouldConnectToWiFi() const;
    bool connectToWiFi();
    void startAccessPoint();
    
    // Orchestration phases
    void waitForPlatformServices();
    void loadAndValidateSettings();
    void initializeBeaconCore();
    void startNetworkServices();
    void startTransmissionScheduler();
    void mainOperationLoop();
    
    // Band selection methods
    void initializeCurrentBand();
    void selectNextBand();
    bool isBandEnabledForCurrentHour(const char* band);
    int getBandInt(const char* band, const char* property, int defaultValue);
    void resetBandTracking();
    int getEnabledBandCount();
    
    AppContext* ctx;
    FSM fsm;
    Scheduler scheduler;
    
    bool running;
    time_t lastTimeSync;
    
    // Band selection state
    BandSelectionMode bandSelectionMode;
    int currentBandIndex;
    char currentBand[8];
    int currentHour;
    bool usedBands[9];  // For tracking used bands in random mode (160m through 10m)
    bool firstTransmission;  // Track if this is the first transmission after initialization
    
    // WSPR modulation state
    WSPREncoder wsprEncoder;
    int currentSymbolIndex;
    uint32_t baseFrequency;
    bool modulationActive;
    
    static constexpr const char* BAND_NAMES[9] = {
        "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m"
    };
    
    static constexpr const char* DEFAULT_SETTINGS_JSON = 
        "{"
        "\"callsign\":\"N0CALL\","
        "\"locator\":\"AA00aa\","
        "\"powerDbm\":10,"
        "\"txIntervalMinutes\":4,"
        "\"bandSelectionMode\":\"sequential\""
        "}";
};