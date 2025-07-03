#include "scheduler-test.cpp"
#include "fsm-test.cpp" 
#include "integrated-test.cpp"
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "========================================\n";
    std::cout << "   WSPR Beacon Component Test Suite    \n";
    std::cout << "========================================\n";
    
    try {
        // Test individual components
        std::cout << "\n>> Running Scheduler Tests...\n";
        SchedulerTestHarness schedulerTests;
        schedulerTests.runAllTests();
        
        std::cout << "\n>> Running FSM Tests...\n";
        FSMTestHarness fsmTests;
        fsmTests.runAllTests();
        
        std::cout << "\n>> Running Integration Tests...\n";
        IntegratedTestHarness integrationTests;
        integrationTests.runAllTests();
        
        std::cout << "\n========================================\n";
        std::cout << "       ALL TESTS PASSED SUCCESSFULLY!   \n";
        std::cout << "========================================\n";
        
        std::cout << "\nComponent Summary:\n";
        std::cout << "✓ Scheduler: WSPR-compliant timing (110.6s duration)\n";
        std::cout << "✓ FSM: Clean state management with validation\n";
        std::cout << "✓ Integration: Components work together correctly\n";
        std::cout << "✓ Time acceleration: Tests run in seconds vs. hours\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "\n❌ TEST FAILURE: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << "\n❌ UNKNOWN TEST FAILURE\n";
        return 1;
    }
}