#include "test-server.h"
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  std::cout << "Starting host test bench web server for WSPR beacon UI..." << std::endl;
  
  // Parse command line arguments
  std::string mockDataFile = "mock-data.txt";  // Default file
  std::string logFile = "";  // Default: no file logging
  int port = 8080;
  double timeScale = 1.0;  // Default: real-time
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--mock-data" && i + 1 < argc) {
      mockDataFile = argv[++i];
    } else if (arg == "--log-file" && i + 1 < argc) {
      logFile = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    } else if (arg == "--time-scale" && i + 1 < argc) {
      timeScale = std::stod(argv[++i]);
      if (timeScale <= 0) {
        std::cerr << "Error: time-scale must be positive\n";
        return 1;
      }
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [options]\n";
      std::cout << "Options:\n";
      std::cout << "  --mock-data <file>  Path to mock data JSON file (default: mock-data.txt)\n";
      std::cout << "  --log-file <file>   Path to detailed operation log file (default: stderr only)\n";
      std::cout << "  --port <port>       Server port (default: 8080)\n";
      std::cout << "  --time-scale <n>    Time acceleration factor (default: 1.0)\n";
      std::cout << "                      > 1.0 = faster, < 1.0 = slower\n";
      std::cout << "                      e.g., 60 = 1 minute real time = 1 hour mock time\n";
      std::cout << "  --help, -h          Show this help message\n";
      return 0;
    }
  }
  
  std::cout << "Using mock data file: " << mockDataFile << std::endl;
  if (!logFile.empty()) {
    std::cout << "Logging to file: " << logFile << std::endl;
  }
  if (timeScale != 1.0) {
    std::cout << "Time scale: " << timeScale << "x (1 real second = " << timeScale << " mock seconds)" << std::endl;
  }
  startTestServer(port, mockDataFile, timeScale, logFile);
  return 0;
}
