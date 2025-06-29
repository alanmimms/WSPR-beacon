#include "AppContext.h"
#include "BeaconFSM.h"
#include <iostream>

int main() {
  AppContext ctx;
  BeaconFSM fsm(&ctx);
  fsm.run();
  return 0;
}