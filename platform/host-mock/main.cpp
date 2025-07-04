#include "test-server.h"
#include <iostream>

int main(int argc, char **argv) {
  std::cout << "Starting host test bench web server for WSPR beacon UI..." << std::endl;
  startTestServer(8080);
  return 0;
}
