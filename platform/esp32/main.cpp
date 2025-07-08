#include "AppContext.h"
#include "Beacon.h"
#include <cstdio>

// Include secrets.h if it exists - it contains WiFi credentials for testing
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

extern "C" void app_main(void) {
  static AppContext ctx;
  Beacon beacon(&ctx);
  beacon.run();
}
