#include "test-server.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

void printUsage(const char* programName) {
  std::cout << "WSPR Beacon Host Mock Testbench\n\n";
  std::cout << "Usage: " << programName << " [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --mock-data <file>        Path to mock data JSON file (default: mock-data.txt)\n";
  std::cout << "  --log-file <file>         Path to detailed operation log file (default: stderr only)\n";
  std::cout << "  --log-verbosity <config>  Configure logging verbosity per subsystem\n";
  std::cout << "                            Format: subsystem.level[,subsystem.level...]\n";
  std::cout << "                            Subsystems: API, WIFI, TX, TIME, SETTINGS, SYSTEM, HTTP\n";
  std::cout << "                            Levels: none, basic, v/verbose, vv/debug, vvv/trace\n";
  std::cout << "                            Examples: --log-verbosity \"*.v\" (all verbose)\n";
  std::cout << "                                      --log-verbosity \"API.vv,WIFI.v,TX.basic\"\n";
  std::cout << "                                      --log-verbosity \"TIME.none,*.basic\"\n";
  std::cout << "  --port <port>             Server port (default: 8080)\n";
  std::cout << "  --time-scale <n>          Time acceleration factor (default: 1.0)\n";
  std::cout << "                            > 1.0 = faster, < 1.0 = slower\n";
  std::cout << "                            e.g., 60 = 1 minute real time = 1 hour mock time\n";
  std::cout << "  --help, -h                Show this help message\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << programName << "                                    # Basic operation\n";
  std::cout << "  " << programName << " --time-scale 60                      # Fast time testing\n";
  std::cout << "  " << programName << " --log-verbosity \"*.v\"                # Verbose all subsystems\n";
  std::cout << "  " << programName << " --log-verbosity \"API.vv,WIFI.none\"    # Debug API, silence WiFi\n";
  std::cout << "  " << programName << " --log-file test.log --time-scale 10  # File logging with 10x time\n";
}

void printError(const std::string& error, const char* programName) {
  std::cerr << "Error: " << error << "\n\n";
  printUsage(programName);
}

bool validateArguments(const std::string& mockDataFile, const std::string& logFile, 
                      const std::string& logVerbosity, int port, double timeScale) {
  if (port < 1 || port > 65535) {
    std::cerr << "Error: Port must be between 1 and 65535, got: " << port << std::endl;
    return false;
  }
  
  if (timeScale <= 0.0) {
    std::cerr << "Error: Time scale must be positive, got: " << timeScale << std::endl;
    return false;
  }
  
  if (timeScale > 10000.0) {
    std::cerr << "Warning: Very high time scale (" << timeScale << "x), system may be unstable" << std::endl;
  }
  
  // Validate log verbosity format if provided
  if (!logVerbosity.empty()) {
    std::istringstream stream(logVerbosity);
    std::string item;
    while (std::getline(stream, item, ',')) {
      item.erase(0, item.find_first_not_of(" \t"));
      item.erase(item.find_last_not_of(" \t") + 1);
      
      if (item.empty()) continue;
      
      size_t dotPos = item.find('.');
      if (dotPos != std::string::npos) {
        std::string subsystem = item.substr(0, dotPos);
        std::string level = item.substr(dotPos + 1);
        
        // Validate subsystem names (allow * for all)
        std::vector<std::string> validSubsystems = {"*", "API", "WIFI", "TX", "TIME", "SETTINGS", "SYSTEM", "HTTP", "FSM", "SCHEDULER", "NETWORK"};
        bool validSubsystem = false;
        for (const auto& valid : validSubsystems) {
          if (subsystem == valid) {
            validSubsystem = true;
            break;
          }
        }
        
        if (!validSubsystem) {
          std::cerr << "Warning: Unknown subsystem '" << subsystem << "' in verbosity config" << std::endl;
        }
        
        // Validate level names
        std::vector<std::string> validLevels = {"none", "basic", "v", "verbose", "vv", "debug", "vvv", "trace", "vvvv"};
        bool validLevel = false;
        for (const auto& valid : validLevels) {
          if (level == valid) {
            validLevel = true;
            break;
          }
        }
        
        if (!validLevel) {
          std::cerr << "Warning: Unknown log level '" << level << "' in verbosity config" << std::endl;
        }
      }
    }
  }
  
  return true;
}

int main(int argc, char **argv) {
  // Parse command line arguments
  std::string mockDataFile = "mock-data.txt";  // Default file
  std::string logFile = "";  // Default: no file logging
  std::string logVerbosity = "";  // Default: use basic logging
  int port = 8080;
  double timeScale = 1.0;  // Default: real-time
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--mock-data" && i + 1 < argc) {
      mockDataFile = argv[++i];
    } else if (arg == "--log-file" && i + 1 < argc) {
      logFile = argv[++i];
    } else if (arg == "--log-verbosity" && i + 1 < argc) {
      logVerbosity = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      try {
        port = std::stoi(argv[++i]);
      } catch (const std::exception& e) {
        printError("Invalid port number: " + std::string(argv[i]), argv[0]);
        return 1;
      }
    } else if (arg == "--time-scale" && i + 1 < argc) {
      try {
        timeScale = std::stod(argv[++i]);
      } catch (const std::exception& e) {
        printError("Invalid time scale: " + std::string(argv[i]), argv[0]);
        return 1;
      }
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else {
      printError("Unknown option: " + arg, argv[0]);
      return 1;
    }
  }
  
  // Validate arguments
  if (!validateArguments(mockDataFile, logFile, logVerbosity, port, timeScale)) {
    return 1;
  }
  
  // Display configuration
  std::cout << "WSPR Beacon Host Mock Testbench\n";
  std::cout << "Configuration:\n";
  std::cout << "  Mock data file: " << mockDataFile << std::endl;
  if (!logFile.empty()) {
    std::cout << "  Log file: " << logFile << std::endl;
  } else {
    std::cout << "  Log output: stderr" << std::endl;
  }
  if (!logVerbosity.empty()) {
    std::cout << "  Log verbosity: " << logVerbosity << std::endl;
  }
  std::cout << "  Server port: " << port << std::endl;
  if (timeScale != 1.0) {
    std::cout << "  Time scale: " << timeScale << "x (1 real second = " << timeScale << " mock seconds)" << std::endl;
  }
  std::cout << std::endl;
  
  startTestServer(port, mockDataFile, timeScale, logFile, logVerbosity);
  return 0;
}