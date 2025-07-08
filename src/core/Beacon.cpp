#include "Beacon.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <algorithm>


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
      bandSelectionMode(BandSelectionMode::SEQUENTIAL),
      currentBandIndex(0),
      currentHour(-1),
      wsprEncoder(),
      currentSymbolIndex(0),
      baseFrequency(0),
      modulationActive(false)
{
    strcpy(currentBand, "20m");  // Default band
    memset(usedBands, 0, sizeof(usedBands));
}

Beacon::~Beacon() {
    stop();
}

void Beacon::run() {
    if (running) return;
    
    running = true;
    initialize();
    
    while (running) {
        ctx->timer->executeWithPreciseTiming([this]() {
            periodicTimeSync();
        }, 100);
    }
}

void Beacon::stop() {
    running = false;
    scheduler.stop();
}

void Beacon::initialize() {
    ctx->logger->logInfo("Beacon initializing...");
    
    // Log current settings from NVS
    if (ctx->settings) {
        char* settingsJson = ctx->settings->toJsonString();
        if (settingsJson) {
            ctx->logger->logInfo("Current settings from NVS:");
            ctx->logger->logInfo(settingsJson);
            free(settingsJson);
        }
    }
    
    // Initialize random number generator for band selection
    srand(time(nullptr));
    
    initializeHardware();
    
    fsm.setStateChangeCallback([this](FSM::NetworkState networkState, FSM::TransmissionState txState) {
        this->onStateChanged(networkState, txState);
    });
    
    scheduler.setTransmissionStartCallback([this]() { this->onTransmissionStart(); });
    scheduler.setTransmissionEndCallback([this]() { this->onTransmissionEnd(); });
    
    initializeWebServer();
    startNetworking();
}

void Beacon::initializeHardware() {
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
    } else {
        ctx->logger->logError("Si5351 interface not available - no RF output possible!");
    }
    
    if (ctx->settings) {
        ctx->logger->logInfo("Settings interface available");
    } else {
        ctx->logger->logWarn("Settings interface not available");
    }
}

void Beacon::initializeWebServer() {
    ctx->logger->logInfo("Beacon::initializeWebServer() called");
    
    if (ctx->webServer) {
        ctx->logger->logInfo("WebServer instance found, starting initialization");
        
        // Mount SPIFFS filesystem first
        if (ctx->fileSystem && !ctx->fileSystem->mount()) {
            ctx->logger->logError("Failed to mount SPIFFS filesystem");
        } else {
            ctx->logger->logInfo("SPIFFS filesystem mounted successfully");
        }
        
        ctx->webServer->setSettingsChangedCallback([this]() { this->onSettingsChanged(); });
        
        // Give WebServer access to Scheduler for countdown API
        ctx->webServer->setScheduler(&scheduler);
        
        ctx->logger->logInfo("About to call ctx->webServer->start()");
        ctx->webServer->start();
        ctx->logger->logInfo("ctx->webServer->start() completed");
    } else {
        ctx->logger->logError("WebServer instance is NULL!");
    }
}

void Beacon::startNetworking() {
    if (shouldConnectToWiFi()) {
        fsm.transitionToStaConnecting();
        if (connectToWiFi()) {
            fsm.transitionToReady();
            scheduler.start();
        } else {
            fsm.transitionToApMode();
            startAccessPoint();
            fsm.transitionToReady();
            scheduler.start();
        }
    } else {
        fsm.transitionToApMode();
        startAccessPoint();
        fsm.transitionToReady();
        scheduler.start();
    }
}

void Beacon::onStateChanged(FSM::NetworkState networkState, FSM::TransmissionState txState) {
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "State: %s / %s", 
        fsm.getNetworkStateString(), fsm.getTransmissionStateString());
    ctx->logger->logInfo(logMsg);
    
    // Update WebServer status via platform interface
    if (ctx->webServer && ctx->settings) {
        const char* freqKey = getBandFrequencyKey(currentBand);
        uint32_t frequency = ctx->settings->getInt(freqKey, 14097100);
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
    ctx->logger->logInfo("Settings changed - restarting scheduler");
    
    scheduler.cancelCurrentTransmission();
    scheduler.stop();
    
    if (fsm.getNetworkState() == FSM::NetworkState::READY) {
        scheduler.start();
    }
}

void Beacon::startTransmission() {
    ctx->logger->logInfo("BEACON", "🟢 TRANSMISSION STARTING...");
    
    // Select the band for this transmission
    selectNextBand();
    
    if (ctx->settings && ctx->si5351) {
        // Get frequency for selected band
        const char* freqKey = getBandFrequencyKey(currentBand);
        uint32_t frequency = ctx->settings->getInt(freqKey, 14097100);  // Default to 20m WSPR
        
        ctx->logger->logInfo("BEACON", "Setting up RF for %s band at %.6f MHz", 
                           currentBand, frequency / 1000000.0);
        
        // Store frequency and enable WSPR modulation
        baseFrequency = frequency;
        
        // Store current band in settings for status display
        ctx->settings->setString("curBand", currentBand);
        ctx->settings->setInt("freq", frequency);
        
        // Start WSPR modulation
        startWSPRModulation();
        
        ctx->logger->logInfo("BEACON", "WSPR modulation started - transmitting encoded message");
    } else {
        ctx->logger->logError("BEACON", "Cannot start transmission - Si5351 or settings not available!");
    }
    
    if (ctx->settings) {
        char logMsg[256];
        const char* freqKey = getBandFrequencyKey(currentBand);
        uint32_t frequency = ctx->settings->getInt(freqKey, 14097100);
        
        snprintf(logMsg, sizeof(logMsg), "🟢 TX START: %s, %s, %ddBm on %s (%.6f MHz)",
            ctx->settings->getString("call", "N0CALL"),
            ctx->settings->getString("loc", "AA00aa"), 
            ctx->settings->getInt("pwr", 10),
            currentBand,
            frequency / 1000000.0
        );
        ctx->logger->logInfo("BEACON", logMsg);
    }
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, false); // LED on (active-low)
        ctx->logger->logInfo("BEACON", "Status LED ON (transmission active)");
    }
}

void Beacon::endTransmission() {
    ctx->logger->logInfo("BEACON", "🔴 TRANSMISSION ENDING...");
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "🔴 TX END on %s after %.1f seconds", 
            currentBand, Scheduler::WSPR_TRANSMISSION_DURATION_SEC);
    ctx->logger->logInfo("BEACON", logMsg);
    
    // Stop WSPR modulation
    stopWSPRModulation();
    
    ctx->logger->logInfo("BEACON", "WSPR modulation stopped - RF output off");
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, true); // LED off (active-low)
        ctx->logger->logInfo("BEACON", "Status LED OFF (transmission complete)");
    }
}

void Beacon::syncTime() {
    ctx->logger->logInfo("Syncing time via SNTP");
    
    if (ctx->timer) {
        ctx->timer->syncTime();
        lastTimeSync = time(nullptr);
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
    for (int i = 0; i < 9; i++) {
        if (isBandEnabledForCurrentHour(BAND_NAMES[i])) {
            enabledBandIndices.push_back(i);
        }
    }
    
    if (enabledBandIndices.empty()) {
        // No bands enabled for this hour
        return;
    }
    
    int selectedIndex = -1;
    
    switch (bandSelectionMode) {
        case BandSelectionMode::SEQUENTIAL:
            // Always use the first enabled band
            selectedIndex = enabledBandIndices[0];
            break;
            
        case BandSelectionMode::ROUND_ROBIN:
            // Find current band in enabled list and move to next
            {
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
    
    if (selectedIndex >= 0 && selectedIndex < 9) {
        currentBandIndex = selectedIndex;
        strcpy(currentBand, BAND_NAMES[selectedIndex]);
        
        // Log band selection
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "Selected band: %s", currentBand);
        ctx->logger->logInfo(logMsg);
    }
}

bool Beacon::isBandEnabledForCurrentHour(const char* band) {
    if (!ctx->settings || !band) return false;
    
    // Build key for band enabled state
    char key[32];
    snprintf(key, sizeof(key), "bands.%s.en", band);
    if (ctx->settings->getInt(key, 0) == 0) {  // 0 = false, 1 = true
        return false;
    }
    
    // Check if band is scheduled for current hour
    snprintf(key, sizeof(key), "bands.%s.sched", band);
    
    // Get current UTC hour
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    int hour = utc->tm_hour;
    
    // Get schedule bitmap (32-bit integer with bits set for active hours)
    uint32_t scheduleBitmap = (uint32_t)ctx->settings->getInt(key, 0xFFFFFF); // Default: all 24 hours enabled
    
    // Check if the current hour bit is set
    return (scheduleBitmap & (1 << hour)) != 0;
}

void Beacon::resetBandTracking() {
    memset(usedBands, 0, sizeof(usedBands));
}

int Beacon::getEnabledBandCount() {
    int count = 0;
    for (int i = 0; i < 9; i++) {
        if (isBandEnabledForCurrentHour(BAND_NAMES[i])) {
            count++;
        }
    }
    return count;
}

const char* Beacon::getBandFrequencyKey(const char* band) {
    static char key[64];
    snprintf(key, sizeof(key), "bands.%s.freq", band);
    return key;
}


void Beacon::startWSPRModulation() {
    if (!ctx->settings || !ctx->si5351 || !ctx->wsprModulator) {
        ctx->logger->logError("BEACON", "Cannot start WSPR modulation - missing components");
        return;
    }
    
    // Get WSPR parameters from settings
    const char* callsign = ctx->settings->getString("call", "N0CALL");
    const char* locator = ctx->settings->getString("loc", "AA00aa");
    int8_t powerDbm = (int8_t)ctx->settings->getInt("pwr", 10);
    
    ctx->logger->logInfo("BEACON", "Encoding WSPR message: %s %s %ddBm", callsign, locator, powerDbm);
    
    // Encode the WSPR message
    wsprEncoder.encode(callsign, locator, powerDbm);
    
    // Reset modulation state
    currentSymbolIndex = 0;
    modulationActive = true;
    
    // Set initial frequency (symbol 0) - ToneSpacing is in centi-Hz, so divide by 100
    uint32_t initialFreq = baseFrequency + (wsprEncoder.symbols[0] * WSPREncoder::ToneSpacing / 100);
    ctx->si5351->setFrequency(0, initialFreq);
    ctx->si5351->enableOutput(0, true);
    
    ctx->logger->logInfo("BEACON", "Starting with symbol %d, freq %.2f Hz offset", 
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
        ctx->logger->logInfo("BEACON", "WSPR modulation started - transmitting encoded message");
    } else {
        ctx->logger->logError("BEACON", "Failed to start WSPR modulation");
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
    
    ctx->logger->logInfo("BEACON", "WSPR modulation stopped after %d symbols", currentSymbolIndex);
}

void Beacon::modulateSymbol(int symbolIndex) {
    if (!modulationActive || !ctx->si5351) {
        return;
    }
    
    // Update current symbol index
    currentSymbolIndex = symbolIndex;
    
    // Check if we've transmitted all symbols
    if (symbolIndex >= 162) {
        ctx->logger->logInfo("BEACON", "All 162 WSPR symbols transmitted");
        return; // Let the main transmission timer handle the end
    }
    
    // Get the current symbol and calculate frequency
    uint8_t symbol = wsprEncoder.symbols[symbolIndex];
    uint32_t symbolFreq = baseFrequency + (symbol * WSPREncoder::ToneSpacing / 100); // Convert centi-Hz to Hz
    
    // Set the new frequency
    ctx->si5351->setFrequency(0, symbolFreq);
    
    // Output symbol letter with newline for immediate output (A=0, B=1, C=2, D=3)
    char symbolChar = 'A' + symbol;
    putchar(symbolChar); fflush(stdout); fsync(fileno(stdout));
}
