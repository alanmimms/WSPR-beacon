#pragma once

#include <functional>

class FSM {
public:
    enum class NetworkState {
        BOOTING,
        AP_MODE,
        STA_CONNECTING,
        READY,
        ERROR
    };

    enum class TransmissionState {
        IDLE,
        TX_PENDING,
        TRANSMITTING
    };

    using StateChangeCallback = std::function<void(NetworkState, TransmissionState)>;

    FSM();

    void setStateChangeCallback(StateChangeCallback callback);

    NetworkState getNetworkState() const;
    TransmissionState getTransmissionState() const;

    void transitionToApMode();
    void transitionToStaConnecting();
    void transitionToReady();
    void transitionToError();

    void transitionToTransmissionPending();
    void transitionToTransmitting();
    void transitionToIdle();

    bool canStartTransmission() const;
    bool isTransmissionActive() const;

    const char* getNetworkStateString() const;
    const char* getTransmissionStateString() const;

private:
    void notifyStateChange();

    NetworkState networkState;
    TransmissionState transmissionState;
    StateChangeCallback onStateChange;
};