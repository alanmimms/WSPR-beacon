#pragma once

#include "AppContext.h"
#include "FSM.h"
#include "Scheduler.h"
#include "JTEncode.h"
#include "Si5351Intf.h"
#include <ctime>

class Beacon {
public:
    enum class BandSelectionMode {
        SEQUENTIAL,      // Single band, no rotation
        ROUND_ROBIN,     // Cycle through bands in order
        RANDOM_EXHAUSTIVE // Random selection until all used
    };

    struct NextTransmissionInfo {
        int secondsUntil;
        char band[8];
        uint32_t frequency;
        bool valid;
    };

    explicit Beacon(AppContext* ctx);
    ~Beacon();

    void run();
    void stop();
    
    // Calibration mode support
    void setCalibrationMode(bool enabled);
    bool isCalibrationMode() const;
    
    // Si5351 access for calibration
    Si5351Intf* getSi5351() const { return ctx ? ctx->si5351 : nullptr; }
    
    // Next transmission prediction for footer display
    NextTransmissionInfo getNextTransmissionInfo() const;

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
    int getBandInt(const char* band, const char* property, int defaultValue) const;
    void resetBandTracking();
    int getEnabledBandCount();
    
    // Next transmission prediction helpers
    bool isBandEnabledForHour(const char* band, int hour) const;
    const char* predictNextBand(time_t futureTime) const;
    
    // Timezone methods (for UI helpers)
    void detectTimezone();
    bool isLocalDaylight(time_t utcTime);
    int getLocalHour(time_t utcTime);
    
    // Transmission statistics tracking
    void incrementTransmissionStats();
    
    AppContext* ctx;
    FSM fsm;
    Scheduler scheduler;
    
    bool running;
    time_t lastTimeSync;
    int timezoneOffset;  // Hours offset from UTC (-12 to +12)
    
    static constexpr const char* BAND_NAMES[12] = {
        "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"
    };
    
    // Band selection state
    BandSelectionMode bandSelectionMode;
    int currentBandIndex;
    char currentBand[8];
    int currentHour;
    bool usedBands[sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0])];  // For tracking used bands in random mode
    bool firstTransmission;  // Track if this is the first transmission after initialization
    
    // WSPR modulation state
    WSPREncoder wsprEncoder;
    int currentSymbolIndex;
    uint32_t baseFrequency;
    bool modulationActive;
    
    static constexpr const char* DEFAULT_SETTINGS_JSON = 
        "{"
        "\"callsign\":\"N0CALL\","
        "\"locator\":\"AA00aa\","
        "\"powerDbm\":10,"
        "\"txIntervalMinutes\":4,"
        "\"bandSelectionMode\":\"sequential\""
        "}";
};