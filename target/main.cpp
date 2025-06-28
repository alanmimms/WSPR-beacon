#include "BeaconFSM.h"
#include "esp_event.h"
#include "esp_netif.h"

static BeaconFSM fsm;

extern "C" void app_main(void) {
  // Initialize TCP/IP stack and event loop. These are prerequisites for WiFi.
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  fsm.run();
}
