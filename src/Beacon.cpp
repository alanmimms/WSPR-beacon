#include "Beacon.h"
#include <cstring>
#include <cstdio>

Beacon::Beacon(AppContext* ctx)
    : ctx(ctx),
      fsm(),
      scheduler(ctx->timer, ctx->settings),
      running(false),
      lastTimeSync(0)
{}

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
    if (ctx->logger && ctx->settings) {
        char logMsg[256];
        snprintf(logMsg, sizeof(logMsg), "TX start: %s, %s, %ddBm, next in %ds",
            ctx->settings->getString("callsign", "N0CALL"),
            ctx->settings->getString("locator", "AA00aa"), 
            ctx->settings->getInt("powerDbm", 10),
            scheduler.getSecondsUntilNextTransmission()
        );
        ctx->logger->logInfo(logMsg);
    }
    
    if (ctx->gpio) {
        ctx->gpio->setOutput(ctx->statusLEDGPIO, true);
    }
    
}

void Beacon::endTransmission() {
    if (ctx->logger) {
        ctx->logger->logInfo("TX end");
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
    
    const char* ssid = ctx->settings->getString("ssid", "");
    return ssid && ssid[0] != '\0';
}

bool Beacon::connectToWiFi() {
    if (!ctx->net || !ctx->settings) return false;
    
    const char* ssid = ctx->settings->getString("ssid", "");
    const char* password = ctx->settings->getString("password", "");
    
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