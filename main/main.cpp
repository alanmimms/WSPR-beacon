#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "BeaconFSM.h"

// Per the project context, C-style function definitions are used where appropriate.
extern "C" {
  void app_main();
}

// Pointer to the main FSM instance.
// This is allocated on the heap in app_main to ensure it's not destroyed
// when the app_main task function exits.
static BeaconFsm* beaconFsm = nullptr;

void app_main() {
  // Instantiate the BeaconFsm class on the heap.
  beaconFsm = new BeaconFsm();

  // Start the state machine. The object will now manage all beacon operations.
  beaconFsm->start();

  // The app_main task can now exit, while the FSM and its related
  // tasks/timers continue to run in the background.
}
