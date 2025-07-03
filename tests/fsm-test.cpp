#include "../include/FSM.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

class FSMTestHarness {
public:
    FSMTestHarness() : fsm(), stateChanges() {
        fsm.setStateChangeCallback([this](FSM::NetworkState netState, FSM::TransmissionState txState) {
            this->onStateChanged(netState, txState);
        });
    }
    
    void testNetworkStateTransitions() {
        std::cout << "\n=== Test: Network State Transitions ===\n";
        
        assert(fsm.getNetworkState() == FSM::NetworkState::BOOTING);
        std::cout << "Initial state: " << fsm.getNetworkStateString() << "\n";
        
        fsm.transitionToApMode();
        assert(fsm.getNetworkState() == FSM::NetworkState::AP_MODE);
        std::cout << "Transitioned to: " << fsm.getNetworkStateString() << "\n";
        
        fsm.transitionToStaConnecting();
        assert(fsm.getNetworkState() == FSM::NetworkState::STA_CONNECTING);
        std::cout << "Transitioned to: " << fsm.getNetworkStateString() << "\n";
        
        fsm.transitionToReady();
        assert(fsm.getNetworkState() == FSM::NetworkState::READY);
        std::cout << "Transitioned to: " << fsm.getNetworkStateString() << "\n";
        
        std::cout << "✓ Network state transitions working correctly\n";
    }
    
    void testTransmissionStateTransitions() {
        std::cout << "\n=== Test: Transmission State Transitions ===\n";
        
        fsm.transitionToReady();
        
        assert(fsm.getTransmissionState() == FSM::TransmissionState::IDLE);
        assert(fsm.canStartTransmission());
        std::cout << "Initial TX state: " << fsm.getTransmissionStateString() << "\n";
        
        fsm.transitionToTransmissionPending();
        assert(fsm.getTransmissionState() == FSM::TransmissionState::TX_PENDING);
        assert(!fsm.canStartTransmission());
        std::cout << "TX state: " << fsm.getTransmissionStateString() << "\n";
        
        fsm.transitionToTransmitting();
        assert(fsm.getTransmissionState() == FSM::TransmissionState::TRANSMITTING);
        assert(fsm.isTransmissionActive());
        std::cout << "TX state: " << fsm.getTransmissionStateString() << "\n";
        
        fsm.transitionToIdle();
        assert(fsm.getTransmissionState() == FSM::TransmissionState::IDLE);
        assert(!fsm.isTransmissionActive());
        std::cout << "TX state: " << fsm.getTransmissionStateString() << "\n";
        
        std::cout << "✓ Transmission state transitions working correctly\n";
    }
    
    void testErrorStateHandling() {
        std::cout << "\n=== Test: Error State Handling ===\n";
        
        fsm.transitionToReady();
        fsm.transitionToTransmissionPending();
        fsm.transitionToTransmitting();
        
        assert(fsm.isTransmissionActive());
        
        fsm.transitionToError();
        assert(fsm.getNetworkState() == FSM::NetworkState::ERROR);
        assert(fsm.getTransmissionState() == FSM::TransmissionState::IDLE);
        assert(!fsm.isTransmissionActive());
        
        std::cout << "After error: Network=" << fsm.getNetworkStateString() 
                  << ", TX=" << fsm.getTransmissionStateString() << "\n";
        
        fsm.transitionToApMode();
        assert(fsm.getNetworkState() == FSM::NetworkState::ERROR);
        std::cout << "Error state is persistent - cannot transition out\n";
        
        std::cout << "✓ Error state handling working correctly\n";
    }
    
    void testStateValidation() {
        std::cout << "\n=== Test: State Validation ===\n";
        
        FSM testFsm;
        
        testFsm.transitionToApMode();
        testFsm.transitionToTransmissionPending();
        assert(testFsm.getTransmissionState() == FSM::TransmissionState::IDLE);
        std::cout << "✓ Cannot start transmission without READY network state\n";
        
        testFsm.transitionToReady();
        testFsm.transitionToTransmissionPending();
        assert(testFsm.getTransmissionState() == FSM::TransmissionState::TX_PENDING);
        std::cout << "✓ Can start transmission when network is READY\n";
        
        testFsm.transitionToTransmitting();
        testFsm.transitionToTransmitting();
        assert(testFsm.getTransmissionState() == FSM::TransmissionState::TRANSMITTING);
        std::cout << "✓ State transitions are idempotent\n";
        
        std::cout << "✓ State validation working correctly\n";
    }
    
    void testCallbackFunctionality() {
        std::cout << "\n=== Test: Callback Functionality ===\n";
        
        stateChanges.clear();
        
        FSM callbackFsm;
        std::vector<std::string> callbackLog;
        
        callbackFsm.setStateChangeCallback([&callbackLog, &callbackFsm](FSM::NetworkState, FSM::TransmissionState) {
            callbackLog.push_back("Network: " + std::string(callbackFsm.getNetworkStateString()) + 
                                 ", TX: " + std::string(callbackFsm.getTransmissionStateString()));
        });
        
        callbackFsm.transitionToApMode();
        callbackFsm.transitionToReady();
        callbackFsm.transitionToTransmissionPending();
        callbackFsm.transitionToTransmitting();
        callbackFsm.transitionToIdle();
        
        assert(callbackLog.size() == 5);
        std::cout << "Callback invocations:\n";
        for (const auto& log : callbackLog) {
            std::cout << "  " << log << "\n";
        }
        
        std::cout << "✓ Callbacks working correctly\n";
    }
    
    void runAllTests() {
        std::cout << "Starting FSM Test Suite\n";
        
        testNetworkStateTransitions();
        testTransmissionStateTransitions();
        testErrorStateHandling();
        testStateValidation();
        testCallbackFunctionality();
        
        std::cout << "\n=== All FSM Tests Passed! ===\n";
        
        std::cout << "\nState change log:\n";
        for (const auto& change : stateChanges) {
            std::cout << "  " << change << "\n";
        }
    }

private:
    void onStateChanged(FSM::NetworkState, FSM::TransmissionState) {
        stateChanges.push_back("Network: " + std::string(fsm.getNetworkStateString()) + 
                              ", TX: " + std::string(fsm.getTransmissionStateString()));
    }
    
    FSM fsm;
    std::vector<std::string> stateChanges;
};