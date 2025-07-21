#include "Beacon.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include "cJSON.h"


static const char tag[] = "Beacon";


// Include secrets.h if it exists - it contains WiFi credentials for testing
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

Beacon::Beacon(AppContext* ctx)
    : ctx(ctx),
      fsm(),
      scheduler(ctx->timer, ctx->settings, ctx->logger),
      running(false),
      lastTimeSync(0),
      timezoneOffset(0),
      bandSelectionMode(BandSelectionMode::SEQUENTIAL),
      currentBandIndex(0),
      currentHour(-1),
      firstTransmission(true),
      wsprEncoder(),
      currentSymbolIndex(0),
      baseFrequency(0),
      modulationActive(false)
{
    strcpy(currentBand, "20m");  // Default fallback band
    currentBandIndex = 4;  // Default fallback index
    memset(usedBands, 0, sizeof(usedBands));
}

Beacon::~Beacon() {
    stop();
}

void Beacon::initializeCurrentBand() {
    if (!ctx->settings) {
        ctx->logger->logWarn(tag, "initializeCurrentBand: No settings available");
        return;
    }
    
    // Get current UTC hour
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    int hour = utc->tm_hour;
    currentHour = hour;
    
    ctx->logger->logInfo(tag, "initializeCurrentBand: Checking bands for UTC hour %d", hour);
    
    // Find first enabled band for current hour
    for (int i = 0; i < (int)(sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0])); i++) {
        if (isBandEnabledForCurrentHour(BAND_NAMES[i])) {
            currentBandIndex = i;
            strcpy(currentBand, BAND_NAMES[i]);
            ctx->logger->logInfo(tag, "Initialized current band to %s for UTC hour %d", 
                               currentBand, hour);
            return;
        }
    }
    
    // If no bands are enabled for current hour, keep default
    ctx->logger->logWarn(tag, "No bands enabled for current hour %d, keeping default band %s", 
                        hour, currentBand);
}

void Beacon::run() {
    if (running) return;
    
    running = true;
    ctx->logger->logInfo("Beacon orchestrator starting...");
    
    try {
        // Phase 1: Platform Services Ready
        waitForPlatformServices();
        
        // Phase 2: Load Configuration  
        loadAndValidateSettings();
        
        // Phase 3: Initialize Core Components
        initializeBeaconCore();
        
        // Phase 4: Start Network Services
        startNetworkServices();
        
        // Phase 5: Begin Transmission Operations
        startTransmissionScheduler();
        
        // Phase 6: Main Operation Loop
        mainOperationLoop();
        
    } catch (...) {
        ctx->logger->logError("Beacon orchestration failed - stopping");
        running = false;
    }
}

void Beacon::stop() {
    running = false;
    scheduler.stop();
}

// Phase 1: Platform Services Ready
void Beacon::waitForPlatformServices() {
    ctx->logger->logInfo("Phase 1: Waiting for platform services...");
    
    // Verify all required platform services are available
    if (!ctx->logger) throw std::runtime_error("Logger service not available");
    if (!ctx->settings) throw std::runtime_error("Settings service not available");
    if (!ctx->timer) throw std::runtime_error("Timer service not available");
    if (!ctx->gpio) throw std::runtime_error("GPIO service not available");
    if (!ctx->si5351) throw std::runtime_error("Si5351 service not available");
    if (!ctx->net) throw std::runtime_error("Network service not available");
    if (!ctx->webServer) throw std::runtime_error("WebServer service not available");
    if (!ctx->wsprModulator) throw std::runtime_error("WSPR modulator service not available");
    
    ctx->logger->logInfo("Phase 1: All platform services ready");
}

// Phase 2: Load Configuration
void Beacon::loadAndValidateSettings() {
    ctx->logger->logInfo("Phase 2: Loading and validating settings...");
    
    // Settings should already be loaded by AppContext, but let's verify
    char* settingsJson = ctx->settings->toJsonString();
    if (!settingsJson) {
        throw std::runtime_error("Failed to load settings");
    }
    
    ctx->logger->logInfo("Current settings:");
    ctx->logger->logInfo(settingsJson);
    free(settingsJson);
    
    // Initialize random number generator for band selection
    srand(time(nullptr));
    
    // Detect timezone for day/night scheduling
    detectTimezone();
    
    // Initialize current band based on settings
    initializeCurrentBand();
    
    ctx->logger->logInfo("Phase 2: Settings loaded and validated");
}

// Phase 3: Initialize Core Components
void Beacon::initializeBeaconCore() {
    ctx->logger->logInfo("Phase 3: Initializing beacon core components...");
    
    // Initialize hardware
    ctx->logger->logInfo("Initializing hardware components...");
    
    if (ctx->gpio) {
        ctx->gpio->init();
        ctx->gpio->setOutput(ctx->statusLEDGPIO, true); // LED off (active-low)
        ctx->logger->logInfo("GPIO initialized, status LED off");
    }
    
    // Initialize Si5351 clock generator
    if (ctx->si5351) {
        ctx->logger->logInfo("Initializing Si5351 clock generator...");
        ctx->si5351->init();
        ctx->logger->logInfo("Si5351 initialization complete");
    }
    
    // Set up FSM callbacks
    fsm.setStateChangeCallback([this](FSM::NetworkState networkState, FSM::TransmissionState txState) {
        this->onStateChanged(networkState, txState);
    });
    
    // Set up scheduler callbacks
    scheduler.setTransmissionStartCallback([this]() { this->onTransmissionStart(); });
    scheduler.setTransmissionEndCallback([this]() { this->onTransmissionEnd(); });
    
    ctx->logger->logInfo("Phase 3: Core components initialized");
}

// Phase 4: Start Network Services
void Beacon::startNetworkServices() {
    ctx->logger->logInfo("Phase 4: Starting network services...");
    
    // Initialize web server
    if (ctx->webServer) {
        ctx->logger->logInfo("Initializing web server...");
        
        // Mount SPIFFS filesystem first
        if (ctx->fileSystem && !ctx->fileSystem->mount()) {
            ctx->logger->logError("Failed to mount SPIFFS filesystem");
        } else {
            ctx->logger->logInfo("SPIFFS filesystem mounted successfully");
        }
        
        ctx->webServer->setSettingsChangedCallback([this]() { this->onSettingsChanged(); });
        ctx->webServer->setScheduler(&scheduler);
        ctx->webServer->setBeacon(this);
        ctx->webServer->start();
        ctx->logger->logInfo("Web server started");
    }
    
    // Start WiFi connection
    if (shouldConnectToWiFi()) {
        fsm.transitionToStaConnecting();
        if (connectToWiFi()) {
            fsm.transitionToReady();
        } else {
            fsm.transitionToApMode();
            startAccessPoint();
            fsm.transitionToReady();
        }
    } else {
        fsm.transitionToApMode();
        startAccessPoint();
        fsm.transitionToReady();
    }
    
    ctx->logger->logInfo("Phase 4: Network services started");
}

// Phase 5: Begin Transmission Operations
void Beacon::startTransmissionScheduler() {
    ctx->logger->logInfo("Phase 5: Starting transmission scheduler...");
    
    // Only start scheduler if we're in ready state
    if (fsm.getNetworkState() == FSM::NetworkState::READY) {
        scheduler.start();
        ctx->logger->logInfo("Transmission scheduler started");
    } else {
        ctx->logger->logWarn("Network not ready - scheduler not started");
    }
    
    ctx->logger->logInfo("Phase 5: Transmission scheduler ready");
}

// Phase 6: Main Operation Loop
void Beacon::mainOperationLoop() {
    ctx->logger->logInfo("Phase 6: Entering main operation loop...");
    
    while (running) {
        ctx->timer->executeWithPreciseTiming([this]() {
            periodicTimeSync();
        }, 100);
    }
    
    ctx->logger->logInfo("Main operation loop exited");
}

void Beacon::onStateChanged(FSM::NetworkState networkState, FSM::TransmissionState txState) {
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "State: %s / %s", 
        fsm.getNetworkStateString(), fsm.getTransmissionStateString());
    ctx->logger->logInfo(logMsg);
    
    // Update WebServer status via platform interface
    if (ctx->webServer && ctx->settings) {
        uint32_t frequency = getBandInt(currentBand, "freq", 14095600);
        ctx->webServer->updateBeaconState(fsm.getNetworkStateString(), fsm.getTransmissionStateString(), currentBand, frequency);
    }
    
    if (networkState == FSM::NetworkState::ERROR) {
        ctx->logger->logError("Entering error state");
        scheduler.stop();
    }
}

void Beacon::onTransmissionStart() {
    if (!fsm.canStartTransmission()) {
        ctx->logger->logWarn("Cannot start transmission in current state");
        return;
    }
    
    fsm.transitionToTransmissionPending();
    startTransmission();
    fsm.transitionToTransmitting();
}

void Beacon::onTransmissionEnd() {
    endTransmission();
    fsm.transitionToIdle();
}

void Beacon::onSettingsChanged() {
    ctx->logger->logInfo("Settings changed - stopping current transmission and applying immediately");
    
    // Stop any active transmission immediately
    if (fsm.getTransmissionState() == FSM::TransmissionState::TRANSMITTING || 
        fsm.getTransmissionState() == FSM::TransmissionState::TX_PENDING) {
        ctx->logger->logInfo("Stopping active transmission to apply new settings");
        
        // Stop WSPR modulation if active
        if (modulationActive) {
            stopWSPRModulation();
        }
        
        // Force transition back to IDLE state
        fsm.transitionToIdle();
    }
    
    // Cancel any scheduled transmissions and stop scheduler
    scheduler.cancelCurrentTransmission();
    scheduler.stop();
    
    try {
        // Re-run configuration phase
        detectTimezone();  // Update timezone first
        firstTransmission = true;  // Reset to start from first enabled band
        initializeCurrentBand();  // Then update band selection
        
        // Restart scheduler if network is ready - this will immediately check for next transmission opportunity
        if (fsm.getNetworkState() == FSM::NetworkState::READY) {
            scheduler.start();
            ctx->logger->logInfo("Scheduler restarted - new settings applied, will transmit on next even minute if scheduled");
        }
    } catch (const std::exception& e) {
        ctx->logger->logError("Failed to apply settings changes: %s", e.what());
    }
}

void Beacon::startTransmission() {
    ctx->logger->logInfo(tag, "🟢 TRANSMISSION STARTING...");
    
    // Select the band for this transmission
    selectNextBand();
    
    if (ctx->settings && ctx->si5351) {
        // Get frequency for selected band
        uint32_t frequency = getBandInt(currentBand, "freq", 14095600);  // Default to 20m WSPR
        
        ctx->logger->logInfo(tag, "Setting up RF for %s band at %.6f MHz", 
                           currentBand, frequency / 1000000.0);
        
        // Store frequency and enable WSPR modulation
        baseFrequency = frequency;
        
        // Store current band in settings for status display
        ctx->settings->setString("curBand", currentBand);
        ctx->settings->setInt("freq", frequency);
        
        // Start WSPR modulation
        startWSPRModulation();
        
        ctx->logger->logInfo(tag, "WSPR modulation started - transmitting encoded message");
    } else {
        ctx->logger->logError(tag, "Cannot start transmission - Si5351 or settings not available!");
    }
    
    if (ctx->settings) {
        char logMsg[256];
        uint32_t frequency = getBandInt(currentBand, "freq", 14095600);
        
        snprintf(logMsg, sizeof(logMsg), "🟢 TX START: %s, %s, %ddBm on %s (%.6f MHz)",
            ctx->settings->getString("call", "N0CALL"),
            ctx->settings->getString("loc", "AA00aa"), 
            ctx->settings->getInt("pwr", 10),
            currentBand,
            frequency / 1000000.0
        );
        ctx->logger->logInfo(tag, logMsg);
    }
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, false); // LED on (active-low)
        ctx->logger->logInfo(tag, "Status LED ON (transmission active)");
    }
}

void Beacon::endTransmission() {
    ctx->logger->logInfo(tag, "🔴 TRANSMISSION ENDING...");
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "🔴 TX END on %s after %.1f seconds", 
            currentBand, Scheduler::WSPR_TRANSMISSION_DURATION_SEC);
    ctx->logger->logInfo(tag, logMsg);
    
    // Stop WSPR modulation
    stopWSPRModulation();
    
    ctx->logger->logInfo(tag, "WSPR modulation stopped - RF output off");
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, true); // LED off (active-low)
        ctx->logger->logInfo(tag, "Status LED OFF (transmission complete)");
    }
    
    // Track transmission statistics after successful completion
    incrementTransmissionStats();
}

void Beacon::syncTime() {
    ctx->logger->logInfo("Syncing time via SNTP");
    
    if (ctx->time) {
        // Use NTP pool servers for time synchronization
        const char* ntpServer = "pool.ntp.org";
        if (ctx->time->syncTime(ntpServer)) {
            lastTimeSync = time(nullptr);
            ctx->logger->logInfo("Time sync initiated with %s", ntpServer);
        } else {
            ctx->logger->logWarn("Failed to initiate time sync");
        }
    }
}

void Beacon::periodicTimeSync() {
    time_t now = time(nullptr);
    if ((now - lastTimeSync) > 3600) {
        syncTime();
    }
}


bool Beacon::shouldConnectToWiFi() const {
    // Always try to connect if hardcoded credentials are available
    #ifdef WIFI_SSID
    ctx->logger->logInfo("Using hardcoded WiFi credentials, forcing STA mode");
    return true;
    #else
    ctx->logger->logInfo("No hardcoded WiFi credentials, checking settings");
    #endif
    
    if (!ctx->settings) return false;
    
    // Check WiFi mode - only connect to existing WiFi in STA mode
    const char* wifiMode = ctx->settings->getString("wifiMode", "sta");
    if (strcmp(wifiMode, "sta") != 0) {
        return false; // AP mode - don't try to connect
    }
    
    const char* ssid = ctx->settings->getString("ssid", "");
    return ssid && ssid[0] != '\0';
}

bool Beacon::connectToWiFi() {
    if (!ctx->net) return false;
    
    const char* ssid = "";
    const char* password = "";
    
    if (ctx->settings) {
        ssid = ctx->settings->getString("ssid", "");
        password = ctx->settings->getString("pwd", "");
    }
    
    // If no settings or empty SSID, try hardcoded credentials for testing
    if (!ssid || ssid[0] == '\0') {
        #ifdef WIFI_SSID
        ssid = WIFI_SSID;
        password = WIFI_PASSWORD;
        ctx->logger->logInfo("Using hardcoded WiFi credentials for testing");
        #else
        return false;
        #endif
    }
    
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "Connecting to WiFi: %s", ssid);
    ctx->logger->logInfo(logMsg);
    
    bool connected = ctx->net->connect(ssid, password);
    
    if (connected) {
        ctx->logger->logInfo("WiFi connected");
        syncTime();
    }
    
    return connected;
}

void Beacon::startAccessPoint() {
    ctx->logger->logInfo("Starting Access Point mode");
    
    if (ctx->net) {
        if (!ctx->net->init()) {
            ctx->logger->logError("Failed to initialize WiFi");
            return;
        }
        ctx->net->startServer(80);
    }
}

// Band selection implementation
void Beacon::selectNextBand() {
    if (!ctx->settings) return;
    
    // Get current UTC hour
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    int hour = utc->tm_hour;
    
    // Check if hour has changed - reset tracking if needed
    if (hour != currentHour) {
        currentHour = hour;
        resetBandTracking();
    }
    
    // Get band selection mode from settings
    const char* modeStr = ctx->settings->getString("bandMode", "sequential");
    if (strcmp(modeStr, "roundRobin") == 0) {
        bandSelectionMode = BandSelectionMode::ROUND_ROBIN;
    } else if (strcmp(modeStr, "randomExhaustive") == 0) {
        bandSelectionMode = BandSelectionMode::RANDOM_EXHAUSTIVE;
    } else {
        bandSelectionMode = BandSelectionMode::SEQUENTIAL;
    }
    
    // Get list of enabled bands for current hour
    std::vector<int> enabledBandIndices;
    for (int i = 0; i < (int)(sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0])); i++) {
        if (isBandEnabledForCurrentHour(BAND_NAMES[i])) {
            enabledBandIndices.push_back(i);
        }
    }
    
    if (enabledBandIndices.empty()) {
        // No bands enabled for this hour
        ctx->logger->logWarn(tag, "No bands enabled for hour %d", hour);
        return;
    }
    
    int selectedIndex = -1;
    
    switch (bandSelectionMode) {
        case BandSelectionMode::SEQUENTIAL:
            // Cycle through enabled bands in order
            {
                auto it = std::find(enabledBandIndices.begin(), enabledBandIndices.end(), currentBandIndex);
                if (it != enabledBandIndices.end() && !firstTransmission) {
                    // Move to next band (but not on first transmission)
                    ++it;
                    if (it == enabledBandIndices.end()) {
                        it = enabledBandIndices.begin();  // Wrap around
                    }
                    selectedIndex = *it;
                } else if (it != enabledBandIndices.end() && firstTransmission) {
                    // First transmission - use current band
                    selectedIndex = currentBandIndex;
                } else {
                    // Current band not in enabled list, start from beginning
                    selectedIndex = enabledBandIndices[0];
                }
            }
            break;
            
        case BandSelectionMode::ROUND_ROBIN:
            // Find current band in enabled list and move to next
            {
                auto it = std::find(enabledBandIndices.begin(), enabledBandIndices.end(), currentBandIndex);
                if (it != enabledBandIndices.end() && !firstTransmission) {
                    // Move to next band (but not on first transmission)
                    ++it;
                    if (it == enabledBandIndices.end()) {
                        it = enabledBandIndices.begin();  // Wrap around
                    }
                    selectedIndex = *it;
                } else if (it != enabledBandIndices.end() && firstTransmission) {
                    // First transmission - use current band
                    selectedIndex = currentBandIndex;
                } else {
                    // Current band not in enabled list, start from beginning
                    selectedIndex = enabledBandIndices[0];
                }
            }
            break;
            
        case BandSelectionMode::RANDOM_EXHAUSTIVE:
            // Select randomly from unused bands
            {
                std::vector<int> unusedEnabledBands;
                for (int idx : enabledBandIndices) {
                    if (!usedBands[idx]) {
                        unusedEnabledBands.push_back(idx);
                    }
                }
                
                if (unusedEnabledBands.empty()) {
                    // All bands used, reset and select from all enabled
                    resetBandTracking();
                    unusedEnabledBands = enabledBandIndices;
                }
                
                // Random selection
                int randomIdx = rand() % unusedEnabledBands.size();
                selectedIndex = unusedEnabledBands[randomIdx];
                usedBands[selectedIndex] = true;
            }
            break;
    }
    
    if (selectedIndex >= 0 && selectedIndex < (int)(sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0]))) {
        currentBandIndex = selectedIndex;
        strcpy(currentBand, BAND_NAMES[selectedIndex]);
        
        // Log band selection
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "Selected band: %s", currentBand);
        ctx->logger->logInfo(logMsg);
        
        // Clear first transmission flag after first selection
        firstTransmission = false;
    }
}

int Beacon::getBandInt(const char* band, const char* property, int defaultValue) const {
    if (!ctx->settings || !band || !property) {
        ctx->logger->logWarn(tag, "getBandInt: Invalid parameters");
        return defaultValue;
    }
    
    // Access nested JSON: settings["bands"][band][property]
    char* settingsJson = ctx->settings->toJsonString();
    if (!settingsJson) {
        ctx->logger->logWarn(tag, "getBandInt: Failed to get settings JSON");
        return defaultValue;
    }
    
    cJSON* root = cJSON_Parse(settingsJson);
    if (!root) {
        ctx->logger->logWarn(tag, "getBandInt: Failed to parse JSON");
        free(settingsJson);
        return defaultValue;
    }
    
    cJSON* bands = cJSON_GetObjectItem(root, "bands");
    if (!bands) {
        ctx->logger->logWarn(tag, "getBandInt: No 'bands' object found");
        cJSON_Delete(root);
        free(settingsJson);
        return defaultValue;
    }
    
    cJSON* bandObj = cJSON_GetObjectItem(bands, band);
    if (!bandObj) {
        ctx->logger->logWarn(tag, "getBandInt: Band '%s' not found", band);
        cJSON_Delete(root);
        free(settingsJson);
        return defaultValue;
    }
    
    cJSON* propObj = cJSON_GetObjectItem(bandObj, property);
    int result = defaultValue;
    if (propObj && cJSON_IsNumber(propObj)) {
        result = propObj->valueint;
    } else if (propObj && cJSON_IsBool(propObj)) {
        result = cJSON_IsTrue(propObj) ? 1 : 0;
    } else {
        ctx->logger->logWarn(tag, "getBandInt: Property '%s' not found or not a number for band '%s'", property, band);
    }
    
    cJSON_Delete(root);
    free(settingsJson);
    return result;
}

bool Beacon::isBandEnabledForCurrentHour(const char* band) {
    if (!ctx->settings || !band) return false;
    
    // Check if band is enabled
    int enabled = getBandInt(band, "en", 0);
    if (enabled == 0) {
        return false;
    }
    
    // Get current UTC hour
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    int hour = utc->tm_hour;
    
    // Get schedule bitmap (32-bit integer with bits set for active hours)
    uint32_t scheduleBitmap = (uint32_t)getBandInt(band, "sched", 0xFFFFFF);
    bool hourEnabled = (scheduleBitmap & (1 << hour)) != 0;
    
    // Only log if band is both enabled and scheduled for this hour
    if (hourEnabled) {
        ctx->logger->logInfo(tag, "  Band %s available for UTC hour %d", band, hour);
    }
    
    return hourEnabled;
}

void Beacon::resetBandTracking() {
    memset(usedBands, 0, sizeof(usedBands));
}

int Beacon::getEnabledBandCount() {
    int count = 0;
    for (int i = 0; i < (int)(sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0])); i++) {
        if (isBandEnabledForCurrentHour(BAND_NAMES[i])) {
            count++;
        }
    }
    return count;
}

// Timezone and day/night methods
void Beacon::detectTimezone() {
    if (!ctx->settings) return;
    
    bool autoTimezone = ctx->settings->getInt("autoTimezone", 1);
    
    if (autoTimezone) {
        // Try to detect timezone from location (maidenhead grid)
        const char* locator = ctx->settings->getString("loc", "AA00aa");
        
        // Simple timezone estimation based on longitude from maidenhead
        // This is a rough approximation - could be improved with a proper timezone database
        if (strlen(locator) >= 4) {
            // Extract longitude from maidenhead grid
            int lonField = (locator[0] - 'A') * 20 + (locator[2] - '0') * 2 - 180;
            int lonSquare = (locator[1] - 'A') * 10 + (locator[3] - '0') - 90;
            
            // Rough longitude (center of grid square)
            float longitude = lonField + lonSquare / 10.0f + 1.0f;
            
            // Estimate timezone (15 degrees per hour)
            timezoneOffset = (int)round(longitude / 15.0f);
            
            // Clamp to reasonable range
            if (timezoneOffset < -12) timezoneOffset = -12;
            if (timezoneOffset > 12) timezoneOffset = 12;
            
            ctx->logger->logInfo(tag, "Auto-detected timezone: UTC%+d from locator %s", timezoneOffset, locator);
        }
    } else {
        // Use manual timezone setting
        const char* timezone = ctx->settings->getString("timezone", "UTC");
        // Parse timezone string (e.g., "UTC+5", "UTC-8", "UTC")
        if (strncmp(timezone, "UTC", 3) == 0) {
            if (strlen(timezone) > 3) {
                timezoneOffset = atoi(&timezone[3]);
            } else {
                timezoneOffset = 0;
            }
        }
        
        ctx->logger->logInfo(tag, "Using manual timezone: %s (UTC%+d)", timezone, timezoneOffset);
    }
}

bool Beacon::isLocalDaylight(time_t utcTime) {
    // Convert UTC to local time
    time_t localTime = utcTime + (timezoneOffset * 3600);
    struct tm* localTm = gmtime(&localTime);
    
    // Simple daylight detection: 6 AM to 6 PM local time
    // This could be improved with sunrise/sunset calculations
    int hour = localTm->tm_hour;
    return (hour >= 6 && hour < 18);
}

int Beacon::getLocalHour(time_t utcTime) {
    time_t localTime = utcTime + (timezoneOffset * 3600);
    struct tm* localTm = gmtime(&localTime);
    return localTm->tm_hour;
}

// Next transmission prediction methods
Beacon::NextTransmissionInfo Beacon::getNextTransmissionInfo() const {
    NextTransmissionInfo info = {0, "", 0, false};
    
    if (!ctx->settings) {
        return info;
    }
    
    // Get seconds until next transmission opportunity
    info.secondsUntil = scheduler.getSecondsUntilNextTransmission();
    
    // Calculate the future time when transmission will occur
    time_t nextTxTime = time(nullptr) + info.secondsUntil;
    
    // Predict which band will be selected for that time
    const char* nextBand = predictNextBand(nextTxTime);
    if (nextBand) {
        strcpy(info.band, nextBand);
        info.frequency = getBandInt(nextBand, "freq", 14095600);
        info.valid = true;
    } else {
        // Fallback to current band if prediction fails
        strcpy(info.band, currentBand);
        info.frequency = getBandInt(currentBand, "freq", 14095600);
        info.valid = false;
    }
    
    return info;
}

bool Beacon::isBandEnabledForHour(const char* band, int hour) const {
    if (!ctx->settings || !band) return false;
    
    // Check if band is enabled
    int enabled = getBandInt(band, "en", 0);
    if (enabled == 0) {
        return false;
    }
    
    // Get schedule bitmap (32-bit integer with bits set for active hours)
    uint32_t scheduleBitmap = (uint32_t)getBandInt(band, "sched", 0xFFFFFF);
    return (scheduleBitmap & (1 << hour)) != 0;
}

const char* Beacon::predictNextBand(time_t futureTime) const {
    if (!ctx->settings) return nullptr;
    
    struct tm* utc = gmtime(&futureTime);
    int futureHour = utc->tm_hour;
    
    // Get band selection mode from settings
    const char* modeStr = ctx->settings->getString("bandMode", "sequential");
    BandSelectionMode mode = BandSelectionMode::SEQUENTIAL;
    if (strcmp(modeStr, "roundRobin") == 0) {
        mode = BandSelectionMode::ROUND_ROBIN;
    } else if (strcmp(modeStr, "randomExhaustive") == 0) {
        mode = BandSelectionMode::RANDOM_EXHAUSTIVE;
    }
    
    // Get list of enabled bands for the future hour
    std::vector<int> enabledBandIndices;
    for (int i = 0; i < (int)(sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0])); i++) {
        if (isBandEnabledForHour(BAND_NAMES[i], futureHour)) {
            enabledBandIndices.push_back(i);
        }
    }
    
    if (enabledBandIndices.empty()) {
        return nullptr; // No bands enabled for that hour
    }
    
    // Predict next band based on mode
    int selectedIndex = -1;
    
    switch (mode) {
        case BandSelectionMode::SEQUENTIAL:
        case BandSelectionMode::ROUND_ROBIN:
            {
                // Find current band in enabled list and advance to next
                auto it = std::find(enabledBandIndices.begin(), enabledBandIndices.end(), currentBandIndex);
                if (it != enabledBandIndices.end()) {
                    // Move to next band
                    ++it;
                    if (it == enabledBandIndices.end()) {
                        it = enabledBandIndices.begin();  // Wrap around
                    }
                    selectedIndex = *it;
                } else {
                    // Current band not in enabled list, start from beginning
                    selectedIndex = enabledBandIndices[0];
                }
            }
            break;
            
        case BandSelectionMode::RANDOM_EXHAUSTIVE:
            {
                // For random mode, we can't predict exactly, so return first available
                // This is a reasonable approximation for footer display
                selectedIndex = enabledBandIndices[0];
            }
            break;
    }
    
    if (selectedIndex >= 0 && selectedIndex < (int)(sizeof(BAND_NAMES) / sizeof(BAND_NAMES[0]))) {
        return BAND_NAMES[selectedIndex];
    }
    
    return nullptr;
}




void Beacon::startWSPRModulation() {
    if (!ctx->settings || !ctx->si5351 || !ctx->wsprModulator) {
        ctx->logger->logError(tag, "Cannot start WSPR modulation - missing components");
        return;
    }
    
    // Get WSPR parameters from settings
    const char* callsign = ctx->settings->getString("call", "N0CALL");
    const char* locator = ctx->settings->getString("loc", "AA00aa");
    int8_t powerDbm = (int8_t)ctx->settings->getInt("pwr", 10);
    
    ctx->logger->logInfo(tag, "Encoding WSPR message: %s %s %ddBm", callsign, locator, powerDbm);
    
    // Encode the WSPR message
    wsprEncoder.encode(callsign, locator, powerDbm);
    
    // Reset modulation state
    currentSymbolIndex = 0;
    modulationActive = true;
    
    // Calculate all 4 WSPR frequencies for glitch-free switching
    double wspr_freqs[4];
    for (int i = 0; i < 4; i++) {
        wspr_freqs[i] = baseFrequency + (i * WSPREncoder::ToneSpacing / 100.0); // Convert centi-Hz to Hz
    }
    
    // Setup Si5351 for glitch-free WSPR frequency transitions
    uint32_t initialFreq = baseFrequency + (wsprEncoder.symbols[0] * WSPREncoder::ToneSpacing / 100);
    ctx->si5351->setupChannelSmooth(0, initialFreq, wspr_freqs);
    ctx->si5351->enableOutput(0, true);
    
    ctx->logger->logInfo(tag, "WSPR frequencies: %.2f, %.2f, %.2f, %.2f Hz", 
                        wspr_freqs[0], wspr_freqs[1], wspr_freqs[2], wspr_freqs[3]);
    
    ctx->logger->logInfo(tag, "Starting with symbol %d, freq %.2f Hz offset", 
                       wsprEncoder.symbols[0], wsprEncoder.symbols[0] * 1.46);
    // Start the symbol stream visualization using printf with newlines for immediate output
    printf("WSPR encoding: ");
    // Output the first symbol (symbol 0)
    char symbolChar = 'A' + wsprEncoder.symbols[0];
    putchar(symbolChar); fflush(stdout); fsync(fileno(stdout));
    
    // Start platform-specific WSPR modulation
    bool started = ctx->wsprModulator->startModulation([this](int symbolIndex) {
        this->modulateSymbol(symbolIndex);
    }, 162);
    
    if (started) {
        ctx->logger->logInfo(tag, "WSPR modulation started - transmitting encoded message");
    } else {
        ctx->logger->logError(tag, "Failed to start WSPR modulation");
        modulationActive = false;
    }
}

void Beacon::stopWSPRModulation() {
    modulationActive = false;
    
    // Stop platform-specific WSPR modulation
    if (ctx->wsprModulator) {
        ctx->wsprModulator->stopModulation();
    }
    
    // Disable Si5351 output
    if (ctx->si5351) {
        ctx->si5351->enableOutput(0, false);
    }
    
    // End symbol stream with newline
    putchar('\n'); fflush(stdout); fsync(fileno(stdout));
    
    ctx->logger->logInfo(tag, "WSPR modulation stopped after %d symbols", currentSymbolIndex);
}

void Beacon::modulateSymbol(int symbolIndex) {
    if (!modulationActive || !ctx->si5351) {
        return;
    }
    
    // Update current symbol index
    currentSymbolIndex = symbolIndex;
    
    // Check if we've transmitted all symbols
    if (symbolIndex >= 162) {
        ctx->logger->logInfo(tag, "All 162 WSPR symbols transmitted");
        return; // Let the main transmission timer handle the end
    }
    
    // Get the current symbol and calculate frequency
    uint8_t symbol = wsprEncoder.symbols[symbolIndex];
    uint32_t symbolFreq = baseFrequency + (symbol * WSPREncoder::ToneSpacing / 100); // Convert centi-Hz to Hz
    
    // Update frequency using glitch-free method
    ctx->si5351->updateChannelFrequencyMinimal(0, symbolFreq);
    
    // Output symbol letter with newline for immediate output (A=0, B=1, C=2, D=3)
    char symbolChar = 'A' + symbol;
    putchar(symbolChar); fflush(stdout); fsync(fileno(stdout));
}

void Beacon::incrementTransmissionStats() {
    if (!ctx->settings) {
        ctx->logger->logWarn(tag, "Cannot update transmission stats - settings not available");
        return;
    }
    
    // Get current statistics (RAM-only, not persisted)
    int totalTxCnt = ctx->settings->getInt("totalTxCnt", 0);
    int totalTxMin = ctx->settings->getInt("totalTxMin", 0);
    
    // Increment totals
    totalTxCnt++;
    totalTxMin += 2; // WSPR transmission is ~2 minutes (110.6 seconds)
    
    // Update totals (RAM-only, no NVS writes)
    ctx->settings->setInt("totalTxCnt", totalTxCnt);
    ctx->settings->setInt("totalTxMin", totalTxMin);
    
    // Update band-specific statistics
    char bandTxCntKey[32], bandTxMinKey[32];
    snprintf(bandTxCntKey, sizeof(bandTxCntKey), "%sTxCnt", currentBand);
    snprintf(bandTxMinKey, sizeof(bandTxMinKey), "%sTxMin", currentBand);
    
    int bandTxCnt = ctx->settings->getInt(bandTxCntKey, 0);
    int bandTxMin = ctx->settings->getInt(bandTxMinKey, 0);
    
    bandTxCnt++;
    bandTxMin += 2;
    
    ctx->settings->setInt(bandTxCntKey, bandTxCnt);
    ctx->settings->setInt(bandTxMinKey, bandTxMin);
    
    // NOTE: Statistics are kept in RAM only to avoid flash wear
    // They reset to zero on reboot, which is acceptable for this use case
    
    ctx->logger->logInfo(tag, "Updated stats (RAM-only): Total TX=%d (%dmins), %s TX=%d (%dmins)", 
                        totalTxCnt, totalTxMin, currentBand, bandTxCnt, bandTxMin);
}

void Beacon::setCalibrationMode(bool enabled) {
    scheduler.setCalibrationMode(enabled);
    if (ctx->logger) {
        ctx->logger->logInfo(tag, "Calibration mode %s", enabled ? "enabled" : "disabled");
    }
}

bool Beacon::isCalibrationMode() const {
    return scheduler.isCalibrationMode();
}
