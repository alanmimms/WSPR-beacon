# WSPR Beacon Component Tests

This test suite validates the refactored WSPR beacon components with time-accelerated simulation.

## Components Tested

### 1. Scheduler
- **WSPR timing compliance**: 110.592 second transmission duration
- **UTC synchronization**: Transmissions start 1 second into even minutes
- **Interval scheduling**: Configurable transmission intervals (2, 4, 6+ minutes)
- **Cancellation handling**: Mid-transmission cancellation support

### 2. FSM (Finite State Machine)
- **Network states**: `BOOTING → AP_MODE → STA_CONNECTING → READY`
- **Transmission states**: `IDLE → TX_PENDING → TRANSMITTING → IDLE`
- **State validation**: Prevents invalid transitions
- **Error handling**: Error state isolation and recovery

### 3. Integration
- **Component coordination**: Scheduler and FSM working together
- **Startup sequence**: Complete device initialization flow
- **Multi-cycle operation**: Multiple transmission cycles
- **Error scenarios**: Network failures and recovery

## Time Acceleration

The `MockTimer` class provides time acceleration up to 200x speed:
- Real 4-minute intervals → 1.2 seconds in test
- Real 110.6s transmission → 0.55 seconds in test
- Complete test suite runs in under 30 seconds

## Running Tests

### Build and Run
```bash
cd tests
mkdir build && cd build
cmake ..
make
./test_runner
```

### Expected Output
```
========================================
   WSPR Beacon Component Test Suite    
========================================

>> Running Scheduler Tests...
=== Test: Basic WSPR Scheduling ===
Next transmission in: 241 seconds
✓ Transmission started as expected
✓ Transmission ended after 110.6 seconds

[... more test output ...]

========================================
       ALL TESTS PASSED SUCCESSFULLY!   
========================================

Component Summary:
✓ Scheduler: WSPR-compliant timing (110.6s duration)
✓ FSM: Clean state management with validation
✓ Integration: Components work together correctly
✓ Time acceleration: Tests run in seconds vs. hours
```

## Test Coverage

### Scheduler Tests
- Basic scheduling and timing
- WSPR protocol compliance
- Interval scheduling (2, 4, 6 minutes)
- Transmission cancellation

### FSM Tests  
- Network state transitions
- Transmission state transitions
- Error state handling
- State validation rules
- Callback functionality

### Integration Tests
- Complete beacon startup sequence
- Full transmission cycles
- Multiple transmission cycles
- Error recovery scenarios
- Scheduler-FSM coordination

## WSPR Protocol Verification

The tests verify compliance with WSPR protocol requirements:

1. **Transmission Duration**: Exactly 110.592 seconds
2. **Start Timing**: Transmissions begin 1 second into even UTC minutes
3. **Interval Compliance**: Configurable intervals (2, 4, 6+ minutes)
4. **UTC Synchronization**: Proper even-minute boundary detection

## Mock Components

### MockTimer
- Time acceleration (1x to 200x speed)
- Timer event simulation
- Activity logging for debugging
- Precise timing control

### Settings (existing)
- JSON-based configuration
- Runtime parameter changes
- Default value handling

This test framework allows rapid validation of timing-critical WSPR protocol behavior without waiting hours for real-time transmission cycles.