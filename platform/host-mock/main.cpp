#include "test-server.h"
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  std::cout << "Starting host test bench web server for WSPR beacon UI..." << std::endl;
  
  // Parse command line arguments
  std::string mockDataFile = "mock-data.txt";  // Default file
  int port = 8080;
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--mock-data" && i + 1 < argc) {
      mockDataFile = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [options]\n";
      std::cout << "Options:\n";
      std::cout << "  --mock-data <file>  Path to mock data JSON file (default: mock-data.txt)\n";
      std::cout << "  --port <port>       Server port (default: 8080)\n";
      std::cout << "  --help, -h          Show this help message\n";
      return 0;
    }
  }
  
  std::cout << "Using mock data file: " << mockDataFile << std::endl;
  startTestServer(port, mockDataFile);
  return 0;
}
