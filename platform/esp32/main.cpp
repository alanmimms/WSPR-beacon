#include "AppContext.h"
#include "Beacon.h"

extern "C" void app_main(void) {
  static AppContext ctx;
  Beacon beacon(&ctx);
  beacon.run();
}