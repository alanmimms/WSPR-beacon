#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <chrono>

// Verbosity levels for logging
enum class LogLevel {
  NONE = 0,     // No logging
  BASIC = 1,    // Basic events only
  VERBOSE = 2,  // Detailed information (.v)
  DEBUG = 3,    // Debug information (.vv) 
  TRACE = 4     // Trace level (.vvv)
};

// Convert verbosity string to LogLevel
LogLevel parseLogLevel(const std::string& levelStr);

// Convert LogLevel to string
std::string logLevelToString(LogLevel level);

#ifdef ESP32_PLATFORM

// ESP32 compile-time logging configuration
// Define these in your build system or main header file:

#ifndef LOG_LEVEL_API
#define LOG_LEVEL_API LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_WIFI  
#define LOG_LEVEL_WIFI LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_TX
#define LOG_LEVEL_TX LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_TIME
#define LOG_LEVEL_TIME LogLevel::NONE
#endif

#ifndef LOG_LEVEL_SETTINGS
#define LOG_LEVEL_SETTINGS LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_SYSTEM
#define LOG_LEVEL_SYSTEM LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_HTTP
#define LOG_LEVEL_HTTP LogLevel::NONE
#endif

#ifndef LOG_LEVEL_FSM
#define LOG_LEVEL_FSM LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_SCHEDULER
#define LOG_LEVEL_SCHEDULER LogLevel::BASIC
#endif

#ifndef LOG_LEVEL_NETWORK
#define LOG_LEVEL_NETWORK LogLevel::BASIC
#endif

#endif // ESP32_PLATFORM

/**
 * BeaconLogger - Comprehensive logging system for WSPR beacon operations
 * 
 * Features:
 * - Per-subsystem verbosity levels (NONE, BASIC, VERBOSE, DEBUG, TRACE)
 * - UTC timestamps with millisecond precision
 * - Thread-safe operation
 * - File logging (host-mock) or stderr (ESP32)
 * - Structured log format with subsystem identification
 * 
 * Usage:
 * - Host: Configure via --log-verbosity command line option
 * - ESP32: Configure via compile-time defines
 */
class BeaconLogger {
private:
  std::unordered_map<std::string, LogLevel> subsystemLevels;
  mutable std::ofstream logFile;
  mutable std::mutex logMutex;
  bool fileLogging;
  LogLevel defaultLevel;
  
  bool shouldLog(const std::string& subsystem, LogLevel messageLevel) const;
  std::string formatTimestamp() const;
  void writeLog(const std::string& logEntry) const;
  
public:
  BeaconLogger(const std::string& logFileName = "", LogLevel defaultLogLevel = LogLevel::BASIC);
  ~BeaconLogger();
  
  // Configure verbosity levels
  void setSubsystemLevel(const std::string& subsystem, LogLevel level);
  void setAllSubsystemsLevel(LogLevel level);
  void parseVerbosityString(const std::string& verbosityConfig);
  
  // Core logging method
  void log(const std::string& subsystem, LogLevel level, const std::string& event, const std::string& data = "");
  
  // Convenience methods for different verbosity levels
  void logBasic(const std::string& subsystem, const std::string& event, const std::string& data = "");
  void logVerbose(const std::string& subsystem, const std::string& event, const std::string& data = "");
  void logDebug(const std::string& subsystem, const std::string& event, const std::string& data = "");
  void logTrace(const std::string& subsystem, const std::string& event, const std::string& data = "");
  
  // Specialized logging methods with automatic verbosity selection
  void logApiRequest(const std::string& method, const std::string& path, int statusCode, const std::string& responseSize = "");
  void logApiRequestVerbose(const std::string& method, const std::string& path, int statusCode, const std::string& headers, const std::string& body);
  
  void logWifiScan(int networkCount, const std::string& details = "");
  void logWifiScanVerbose(int networkCount, const std::string& details, const std::string& timingInfo);
  
  void logTransmissionEvent(const std::string& event, const std::string& band, int64_t nextTxIn = -1);
  void logTransmissionVerbose(const std::string& event, const std::string& band, double frequency, int powerDbm, const std::string& timingDetails);
  
  void logTimeEvent(const std::string& event, double timeScale, int64_t mockTime);
  void logTimeEventVerbose(const std::string& event, double timeScale, int64_t mockTime, int64_t realTime, const std::string& calculations);
  
  void logSettingsChange(const std::string& field, const std::string& oldValue, const std::string& newValue);
  void logSettingsVerbose(const std::string& operation, const std::string& source, int fieldCount, const std::string& fieldList);
  
  void logSystemEvent(const std::string& event, const std::string& data = "");
  void logSystemVerbose(const std::string& event, const std::string& data, const std::string& memoryInfo);
  
  void logNetworkEvent(const std::string& event, const std::string& data = "");
  void logFsmEvent(const std::string& event, const std::string& data = "");
  void logSchedulerEvent(const std::string& event, const std::string& data = "");
  
  // Get current configuration
  std::string getConfigurationSummary() const;
};