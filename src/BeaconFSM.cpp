#include "BeaconFSM.h"
#include <cstring>
#include <cstdio>
#include <ctime>

static const char *defaultSettingsJson =
  "{"
    "\"callsign\":\"N0CALL\","
    "\"locator\":\"AA00aa\","
    "\"powerDbm\":10,"
    "\"txIntervalMinutes\":4"
  "}";

BeaconFSM::BeaconFSM(AppContext *ctx)
  : currentState(State::BOOTING),
    txCanceled(false),
    txInProgress(false),
    ctx(ctx),
    txDurationTimer(nullptr),
    txScheduleTimer(nullptr)
{}

BeaconFSM::~BeaconFSM() {
  if (ctx->timer && txDurationTimer) ctx->timer->destroy(txDurationTimer);
  if (ctx->timer && txScheduleTimer) ctx->timer->destroy(txScheduleTimer);
}

void BeaconFSM::initHardware() {
  if (ctx->settings) {
    // Load settings if needed
  } else if (ctx->logger) {
    ctx->logger->logWarn("Settings pointer not provided in AppContext.");
  }

  if (ctx->gpio) {
    ctx->gpio->init();
    ctx->gpio->setOutput(ctx->statusLEDGPIO, false);
  }
}

void BeaconFSM::run() {
  while (true) {
    switch (currentState) {
      case State::BOOTING:        handleBooting();        break;
      case State::AP_MODE:        handleApMode();         break;
      case State::STA_CONNECTING: handleStaConnecting();  break;
      case State::BEACONING:      handleBeaconing();      break;
      case State::TX_START:       handleTxStart();        break;
      case State::TX_END:         handleTxEnd();          break;
      case State::ERROR:
        if (ctx->logger) ctx->logger->logError("Entering error state. Halting.");
        while (true) ctx->timer->delayMs(1000);
    }
    ctx->timer->delayMs(50);
  }
}

void BeaconFSM::handleBooting() {
  if (ctx->logger) ctx->logger->logInfo("State: BOOTING");
  initHardware();

  if (ctx->webServer) {
    ctx->webServer->setSettingsChangedCallback([this]() { this->onSettingsChanged(); });
    ctx->webServer->start();
  }

  // WiFi credentials check via settings abstraction:
  const char* ssid = "";
  if (ctx->settings) ssid = ctx->settings->getString("ssid", "");
  if (ssid[0]) currentState = State::STA_CONNECTING;
  else currentState = State::AP_MODE;
}

void BeaconFSM::handleApMode() {
  if (ctx->logger) ctx->logger->logInfo("State: AP_MODE");
  if (ctx->net) ctx->net->startServer(80); // start AP-mode server if needed
  if (ctx->webServer) ctx->webServer->start();
  currentState = State::BEACONING;
}

void BeaconFSM::handleStaConnecting() {
  if (ctx->logger) ctx->logger->logInfo("State: STA_CONNECTING");
  const char* ssid = "";
  const char* password = "";
  if (ctx->settings) {
    ssid = ctx->settings->getString("ssid", "");
    password = ctx->settings->getString("password", "");
  }
  bool connected = ctx->net && ctx->net->connect(ssid, password);
  if (connected) {
    if (ctx->webServer) ctx->webServer->start();
    currentState = State::BEACONING;
  } else {
    if (ctx->webServer) ctx->webServer->stop();
    if (ctx->net) ctx->net->disconnect();
    currentState = State::AP_MODE;
  }
}

void BeaconFSM::handleBeaconing() {
  if (ctx->logger) ctx->logger->logInfo("State: BEACONING. Scheduling next transmit if allowed.");
  txCanceled = false;
  txInProgress = false;
  scheduleNextTransmit();
  while (currentState == State::BEACONING) {
    ctx->timer->delayMs(50);
  }
}

void BeaconFSM::handleTxStart() {
  if (ctx->logger) ctx->logger->logInfo("State: TX_START");
  txCanceled = false;
  txInProgress = true;
  startTransmit();

  int txDurationSec = 110;
  if (ctx->timer) {
    if (!txDurationTimer) {
      txDurationTimer = ctx->timer->createOneShot([this]() { this->onTxDurationTimeout(); });
    }
    ctx->timer->start(txDurationTimer, txDurationSec * 1000);
  }

  while (currentState == State::TX_START) {
    ctx->timer->delayMs(20);
  }
}

void BeaconFSM::handleTxEnd() {
  if (ctx->logger) ctx->logger->logInfo("State: TX_END");
  endTransmit();
  txInProgress = false;
  scheduleNextTransmit();
  currentState = State::BEACONING;
}

void BeaconFSM::startTransmit() {
  if (ctx->logger && ctx->settings) {
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "TX cycle: %s, %s, %ddBm.",
      ctx->settings->getString("callsign"), ctx->settings->getString("locator"),
      ctx->settings->getInt("powerDbm", 10)
    );
    ctx->logger->logInfo(logMsg);
  }
  if (ctx->gpio) ctx->gpio->setOutput(ctx->statusLEDGPIO, true);
  // TODO: Call RF hardware via ctx->si5351 or other abstractions
}

void BeaconFSM::endTransmit() {
  if (ctx->logger) ctx->logger->logInfo("TX end (normal/cancel).");
  if (ctx->gpio) ctx->gpio->setOutput(ctx->statusLEDGPIO, false);
}

void BeaconFSM::scheduleNextTransmit() {
  time_t now = time(NULL);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  int interval = ctx->settings ? ctx->settings->getInt("txIntervalMinutes", 4) : 4;
  int secondsPastMinute = tmNow.tm_sec;
  int minutesPastHour = tmNow.tm_min % interval;
  int secondsToNextTx = ((interval - minutesPastHour) * 60) - secondsPastMinute;
  if (secondsToNextTx < 0) secondsToNextTx += interval * 60;
  if (secondsToNextTx < 5) secondsToNextTx += interval * 60;
  if (ctx->logger) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Next transmit in %d seconds.", secondsToNextTx);
    ctx->logger->logInfo(buf);
  }
  if (ctx->timer) {
    if (!txScheduleTimer) {
      txScheduleTimer = ctx->timer->createOneShot([this]() { this->onTxScheduleTimeout(); });
    }
    ctx->timer->start(txScheduleTimer, secondsToNextTx * 1000);
  }
}

void BeaconFSM::onTxDurationTimeout() {
  if (!txCanceled) currentState = State::TX_END;
}

void BeaconFSM::onTxScheduleTimeout() {
  if (!txInProgress && canTransmitNow()) currentState = State::TX_START;
}

void BeaconFSM::cancelTransmit() {
  if (txInProgress) {
    txCanceled = true;
    if (ctx->timer && txDurationTimer) ctx->timer->stop(txDurationTimer);
    currentState = State::TX_END;
  }
}

void BeaconFSM::onSettingsChanged() {
  if (ctx->logger) ctx->logger->logInfo("Settings changed via WebServer.");
  cancelTransmit();
  if (canTransmitNow()) {
    currentState = State::BEACONING;
  }
}

bool BeaconFSM::canTransmitNow() {
  time_t now = time(NULL);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  int interval = ctx->settings ? ctx->settings->getInt("txIntervalMinutes", 4) : 4;
  return (tmNow.tm_min % interval) == 0 && tmNow.tm_sec < 2;
}

void BeaconFSM::syncTime() {
  if (ctx->logger) ctx->logger->logInfo("Starting SNTP time sync...");
  if (ctx->timer) ctx->timer->syncTime();
}

void BeaconFSM::periodicTimeSync() {
  static time_t lastSync = 0;
  time_t now = time(NULL);
  if ((now - lastSync) > 3600 && ctx->timer) {
    syncTime();
    lastSync = now;
  }
}