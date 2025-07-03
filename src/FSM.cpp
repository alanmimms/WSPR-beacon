#include "FSM.h"

FSM::FSM()
    : networkState(NetworkState::BOOTING),
      transmissionState(TransmissionState::IDLE)
{}

void FSM::setStateChangeCallback(StateChangeCallback callback) {
    onStateChange = callback;
}

FSM::NetworkState FSM::getNetworkState() const {
    return networkState;
}

FSM::TransmissionState FSM::getTransmissionState() const {
    return transmissionState;
}

void FSM::transitionToApMode() {
    if (networkState != NetworkState::ERROR) {
        networkState = NetworkState::AP_MODE;
        notifyStateChange();
    }
}

void FSM::transitionToStaConnecting() {
    if (networkState != NetworkState::ERROR) {
        networkState = NetworkState::STA_CONNECTING;
        notifyStateChange();
    }
}

void FSM::transitionToReady() {
    if (networkState != NetworkState::ERROR) {
        networkState = NetworkState::READY;
        notifyStateChange();
    }
}

void FSM::transitionToError() {
    networkState = NetworkState::ERROR;
    transmissionState = TransmissionState::IDLE;
    notifyStateChange();
}

void FSM::transitionToTransmissionPending() {
    if (networkState == NetworkState::READY && transmissionState == TransmissionState::IDLE) {
        transmissionState = TransmissionState::TX_PENDING;
        notifyStateChange();
    }
}

void FSM::transitionToTransmitting() {
    if (transmissionState == TransmissionState::TX_PENDING) {
        transmissionState = TransmissionState::TRANSMITTING;
        notifyStateChange();
    }
}

void FSM::transitionToIdle() {
    if (transmissionState != TransmissionState::IDLE) {
        transmissionState = TransmissionState::IDLE;
        notifyStateChange();
    }
}

bool FSM::canStartTransmission() const {
    return networkState == NetworkState::READY && transmissionState == TransmissionState::IDLE;
}

bool FSM::isTransmissionActive() const {
    return transmissionState == TransmissionState::TX_PENDING || 
           transmissionState == TransmissionState::TRANSMITTING;
}

const char* FSM::getNetworkStateString() const {
    switch (networkState) {
        case NetworkState::BOOTING:        return "BOOTING";
        case NetworkState::AP_MODE:        return "AP_MODE";  
        case NetworkState::STA_CONNECTING: return "STA_CONNECTING";
        case NetworkState::READY:          return "READY";
        case NetworkState::ERROR:          return "ERROR";
        default:                           return "UNKNOWN";
    }
}

const char* FSM::getTransmissionStateString() const {
    switch (transmissionState) {
        case TransmissionState::IDLE:        return "IDLE";
        case TransmissionState::TX_PENDING:  return "TX_PENDING";
        case TransmissionState::TRANSMITTING: return "TRANSMITTING";
        default:                             return "UNKNOWN";
    }
}

void FSM::notifyStateChange() {
    if (onStateChange) {
        onStateChange(networkState, transmissionState);
    }
}