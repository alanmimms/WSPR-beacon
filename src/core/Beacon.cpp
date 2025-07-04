#include "Beacon.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>

Beacon::Beacon(AppContext* ctx)
    : ctx(ctx),
      fsm(),
      scheduler(ctx->timer, ctx->settings),
      running(false),
      lastTimeSync(0),
      bandSelectionMode(BandSelectionMode::SEQUENTIAL),
      currentBandIndex(0),
      currentHour(-1)
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
        periodicTimeSync();
        ctx->timer->delayMs(100);
    }
}

void Beacon::stop() {
    running = false;
    scheduler.stop();
}

void Beacon::initialize() {
    if (ctx->logger) {
        ctx->logger->logInfo("Beacon initializing...");
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
    if (ctx->gpio) {
        ctx->gpio->init();
        ctx->gpio->setOutput(ctx->statusLEDGPIO, false);
    }
    
    if (ctx->settings) {
        
    } else if (ctx->logger) {
        ctx->logger->logWarn("Settings interface not available");
    }
}

void Beacon::initializeWebServer() {
    if (ctx->webServer) {
        ctx->webServer->setSettingsChangedCallback([this]() { this->onSettingsChanged(); });
        ctx->webServer->start();
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
    if (ctx->logger) {
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "State: %s / %s", 
            fsm.getNetworkStateString(), fsm.getTransmissionStateString());
        ctx->logger->logInfo(logMsg);
    }
    
    if (networkState == FSM::NetworkState::ERROR) {
        if (ctx->logger) {
            ctx->logger->logError("Entering error state");
        }
        scheduler.stop();
    }
}

void Beacon::onTransmissionStart() {
    if (!fsm.canStartTransmission()) {
        if (ctx->logger) {
            ctx->logger->logWarn("Cannot start transmission in current state");
        }
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
    if (ctx->logger) {
        ctx->logger->logInfo("Settings changed - restarting scheduler");
    }
    
    scheduler.cancelCurrentTransmission();
    scheduler.stop();
    
    if (fsm.getNetworkState() == FSM::NetworkState::READY) {
        scheduler.start();
    }
}

void Beacon::startTransmission() {
    // Select the band for this transmission
    selectNextBand();
    
    if (ctx->settings && ctx->si5351) {
        // Get frequency for selected band
        const char* freqKey = getBandFrequencyKey(currentBand);
        uint32_t frequency = ctx->settings->getInt(freqKey, 14097100);  // Default to 20m WSPR
        
        // Set the frequency on the Si5351
        ctx->si5351->setFrequency(0, frequency);  // Clock 0
        ctx->si5351->enableOutput(0, true);
        
        // Store current band in settings for status display
        ctx->settings->setString("curBand", currentBand);
        ctx->settings->setInt("freq", frequency);
    }
    
    if (ctx->logger && ctx->settings) {
        char logMsg[256];
        const char* freqKey = getBandFrequencyKey(currentBand);
        uint32_t frequency = ctx->settings->getInt(freqKey, 14097100);
        
        snprintf(logMsg, sizeof(logMsg), "TX start: %s, %s, %ddBm on %s (%.6f MHz)",
            ctx->settings->getString("call", "N0CALL"),
            ctx->settings->getString("loc", "AA00aa"), 
            ctx->settings->getInt("pwr", 10),
            currentBand,
            frequency / 1000000.0
        );
        ctx->logger->logInfo(logMsg);
    }
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, true);
    }
}

void Beacon::endTransmission() {
    if (ctx->logger) {
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "TX end on %s", currentBand);
        ctx->logger->logInfo(logMsg);
    }
    
    if (ctx->si5351) {
        ctx->si5351->enableOutput(0, false);  // Disable clock 0
    }
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, false);
    }
}

void Beacon::syncTime() {
    if (ctx->logger) {
        ctx->logger->logInfo("Syncing time via SNTP");
    }
    
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
    if (!ctx->net || !ctx->settings) return false;
    
    const char* ssid = ctx->settings->getString("ssid", "");
    const char* password = ctx->settings->getString("pwd", "");
    
    if (ctx->logger) {
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "Connecting to WiFi: %s", ssid);
        ctx->logger->logInfo(logMsg);
    }
    
    bool connected = ctx->net->connect(ssid, password);
    
    if (connected && ctx->logger) {
        ctx->logger->logInfo("WiFi connected");
        syncTime();
    }
    
    return connected;
}

void Beacon::startAccessPoint() {
    if (ctx->logger) {
        ctx->logger->logInfo("Starting Access Point mode");
    }
    
    if (ctx->net) {
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
        if (ctx->logger) {
            char logMsg[128];
            snprintf(logMsg, sizeof(logMsg), "Selected band: %s", currentBand);
            ctx->logger->logInfo(logMsg);
        }
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
    
    // TODO: Parse schedule array from JSON settings
    // For now, assume all enabled bands are active all hours
    return true;
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