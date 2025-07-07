#include "BeaconLogger.h"
#include <iostream>
#include <algorithm>
#include <cctype>

LogLevel parseLogLevel(const std::string& levelStr) {
  std::string lower = levelStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  
  if (lower.empty() || lower == "none" || lower == "0") return LogLevel::NONE;
  if (lower == "basic" || lower == "1" || lower == "b") return LogLevel::BASIC;
  if (lower == "verbose" || lower == "2" || lower == "v") return LogLevel::VERBOSE;
  if (lower == "debug" || lower == "3" || lower == "vv") return LogLevel::DEBUG;
  if (lower == "trace" || lower == "4" || lower == "vvv" || lower == "vvvv") return LogLevel::TRACE;
  
  return LogLevel::BASIC; // Default fallback
}

std::string logLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::NONE: return "NONE";
    case LogLevel::BASIC: return "BASIC";
    case LogLevel::VERBOSE: return "VERBOSE";
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::TRACE: return "TRACE";
    default: return "UNKNOWN";
  }
}

BeaconLogger::BeaconLogger(const std::string& logFileName, LogLevel defaultLogLevel) 
  : fileLogging(false), defaultLevel(defaultLogLevel) {
  
#ifdef ESP32_PLATFORM
  // ESP32: Use compile-time configuration
  subsystemLevels["API"] = LOG_LEVEL_API;
  subsystemLevels["WIFI"] = LOG_LEVEL_WIFI;
  subsystemLevels["TX"] = LOG_LEVEL_TX;
  subsystemLevels["TIME"] = LOG_LEVEL_TIME;
  subsystemLevels["SETTINGS"] = LOG_LEVEL_SETTINGS;
  subsystemLevels["SYSTEM"] = LOG_LEVEL_SYSTEM;
  subsystemLevels["HTTP"] = LOG_LEVEL_HTTP;
  subsystemLevels["FSM"] = LOG_LEVEL_FSM;
  subsystemLevels["SCHEDULER"] = LOG_LEVEL_SCHEDULER;
  subsystemLevels["NETWORK"] = LOG_LEVEL_NETWORK;
  
  logBasic("SYSTEM", "Logger initialized for ESP32", "compile_time_config=true");
#else
  // Host: Use file logging if specified
  fileLogging = !logFileName.empty();
  if (fileLogging) {
    logFile.open(logFileName, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
      std::cerr << "Error: Could not open log file: " << logFileName << std::endl;
      fileLogging = false;
    } else {
      logBasic("SYSTEM", "Logger initialized", "file=" + logFileName + ", default_level=" + logLevelToString(defaultLevel));
    }
  } else {
    logBasic("SYSTEM", "Logger initialized", "output=stderr, default_level=" + logLevelToString(defaultLevel));
  }
#endif
}

BeaconLogger::~BeaconLogger() {
  logBasic("SYSTEM", "Logger shutdown", "");
  if (fileLogging && logFile.is_open()) {
    logFile.close();
  }
}

bool BeaconLogger::shouldLog(const std::string& subsystem, LogLevel messageLevel) const {
  if (messageLevel == LogLevel::NONE) return false;
  
  auto it = subsystemLevels.find(subsystem);
  LogLevel subsystemLevel = (it != subsystemLevels.end()) ? it->second : defaultLevel;
  
  return messageLevel <= subsystemLevel;
}

std::string BeaconLogger::formatTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  
  std::tm utc_tm;
  #ifdef _WIN32
  gmtime_s(&utc_tm, &time_t_now);
  #else
  gmtime_r(&time_t_now, &utc_tm);
  #endif
  
  std::ostringstream timestamp;
  timestamp << std::put_time(&utc_tm, "%Y-%m-%d %H:%M:%S");
  timestamp << "." << std::setfill('0') << std::setw(3) << ms.count();
  timestamp << " UTC";
  
  return timestamp.str();
}

void BeaconLogger::writeLog(const std::string& logEntry) const {
  std::lock_guard<std::mutex> lock(logMutex);
  
#ifdef ESP32_PLATFORM
  // ESP32: Always write to stderr (serial console)
  std::cerr << logEntry << std::endl;
#else
  // Host: Write to file if configured, otherwise stderr
  if (fileLogging && logFile.is_open()) {
    logFile << logEntry << std::endl;
    logFile.flush();  // Ensure immediate write
  } else {
    std::cerr << logEntry << std::endl;
  }
#endif
}

void BeaconLogger::setSubsystemLevel(const std::string& subsystem, LogLevel level) {
  subsystemLevels[subsystem] = level;
}

void BeaconLogger::setAllSubsystemsLevel(LogLevel level) {
  defaultLevel = level;
  // Update all existing subsystems
  for (auto& pair : subsystemLevels) {
    pair.second = level;
  }
}

void BeaconLogger::parseVerbosityString(const std::string& verbosityConfig) {
  if (verbosityConfig.empty()) return;
  
  // Split by comma for multiple subsystem configurations
  std::istringstream stream(verbosityConfig);
  std::string item;
  
  while (std::getline(stream, item, ',')) {
    // Trim whitespace
    item.erase(0, item.find_first_not_of(" \t"));
    item.erase(item.find_last_not_of(" \t") + 1);
    
    if (item.empty()) continue;
    
    // Find the dot separator
    size_t dotPos = item.find('.');
    if (dotPos == std::string::npos) {
      // No dot, treat as subsystem with basic level
      setSubsystemLevel(item, LogLevel::BASIC);
      logDebug("SYSTEM", "Verbosity configured", "subsystem=" + item + ", level=BASIC");
      continue;
    }
    
    std::string subsystem = item.substr(0, dotPos);
    std::string levelStr = item.substr(dotPos + 1);
    
    LogLevel level = parseLogLevel(levelStr);
    
    if (subsystem == "*") {
      setAllSubsystemsLevel(level);
      logDebug("SYSTEM", "Verbosity configured for all", "level=" + logLevelToString(level));
    } else {
      setSubsystemLevel(subsystem, level);
      logDebug("SYSTEM", "Verbosity configured", "subsystem=" + subsystem + ", level=" + logLevelToString(level));
    }
  }
}

void BeaconLogger::log(const std::string& subsystem, LogLevel level, const std::string& event, const std::string& data) {
  if (!shouldLog(subsystem, level)) return;
  
  std::ostringstream logEntry;
  logEntry << formatTimestamp();
  logEntry << " [" << subsystem;
  
  // Add level indicator for non-basic levels
  if (level != LogLevel::BASIC) {
    logEntry << ":" << logLevelToString(level);
  }
  
  logEntry << "] " << event;
  if (!data.empty()) {
    logEntry << " | " << data;
  }
  
  writeLog(logEntry.str());
}

// Convenience methods
void BeaconLogger::logBasic(const std::string& subsystem, const std::string& event, const std::string& data) {
  log(subsystem, LogLevel::BASIC, event, data);
}

void BeaconLogger::logVerbose(const std::string& subsystem, const std::string& event, const std::string& data) {
  log(subsystem, LogLevel::VERBOSE, event, data);
}

void BeaconLogger::logDebug(const std::string& subsystem, const std::string& event, const std::string& data) {
  log(subsystem, LogLevel::DEBUG, event, data);
}

void BeaconLogger::logTrace(const std::string& subsystem, const std::string& event, const std::string& data) {
  log(subsystem, LogLevel::TRACE, event, data);
}

// Specialized logging methods
void BeaconLogger::logApiRequest(const std::string& method, const std::string& path, int statusCode, const std::string& responseSize) {
  std::string data = "method=" + method + ", status=" + std::to_string(statusCode);
  if (!responseSize.empty()) {
    data += ", response_size=" + responseSize;
  }
  logBasic("API", "Request: " + path, data);
}

void BeaconLogger::logApiRequestVerbose(const std::string& method, const std::string& path, int statusCode, const std::string& headers, const std::string& body) {
  std::string data = "method=" + method + ", status=" + std::to_string(statusCode);
  if (!headers.empty()) {
    data += ", headers=" + headers;
  }
  if (!body.empty()) {
    data += ", body_preview=" + body.substr(0, 100) + (body.length() > 100 ? "..." : "");
  }
  logVerbose("API", "Request details: " + path, data);
}

void BeaconLogger::logWifiScan(int networkCount, const std::string& details) {
  std::string data = "networks_found=" + std::to_string(networkCount);
  if (!details.empty()) {
    data += ", " + details;
  }
  logBasic("WIFI", "Scan completed", data);
}

void BeaconLogger::logWifiScanVerbose(int networkCount, const std::string& details, const std::string& timingInfo) {
  std::string data = "networks_found=" + std::to_string(networkCount);
  if (!details.empty()) {
    data += ", networks=" + details;
  }
  if (!timingInfo.empty()) {
    data += ", " + timingInfo;
  }
  logVerbose("WIFI", "Scan completed with details", data);
}

void BeaconLogger::logTransmissionEvent(const std::string& event, const std::string& band, int64_t nextTxIn) {
  std::string data = "band=" + band;
  if (nextTxIn >= 0) {
    data += ", next_tx_in=" + std::to_string(nextTxIn) + "s";
  }
  logBasic("TX", event, data);
}

void BeaconLogger::logTransmissionVerbose(const std::string& event, const std::string& band, double frequency, int powerDbm, const std::string& timingDetails) {
  std::string data = "band=" + band + ", freq=" + std::to_string(frequency) + "Hz, power=" + std::to_string(powerDbm) + "dBm";
  if (!timingDetails.empty()) {
    data += ", " + timingDetails;
  }
  logVerbose("TX", event + " details", data);
}

void BeaconLogger::logTimeEvent(const std::string& event, double timeScale, int64_t mockTime) {
  std::string data = "time_scale=" + std::to_string(timeScale) + ", mock_time=" + std::to_string(mockTime);
  logBasic("TIME", event, data);
}

void BeaconLogger::logTimeEventVerbose(const std::string& event, double timeScale, int64_t mockTime, int64_t realTime, const std::string& calculations) {
  std::string data = "time_scale=" + std::to_string(timeScale) + ", mock_time=" + std::to_string(mockTime) + ", real_time=" + std::to_string(realTime);
  if (!calculations.empty()) {
    data += ", " + calculations;
  }
  logVerbose("TIME", event + " calculations", data);
}

void BeaconLogger::logSettingsChange(const std::string& field, const std::string& oldValue, const std::string& newValue) {
  std::string data = "field=" + field + ", old=" + oldValue + ", new=" + newValue;
  logBasic("SETTINGS", "Configuration changed", data);
}

void BeaconLogger::logSettingsVerbose(const std::string& operation, const std::string& source, int fieldCount, const std::string& fieldList) {
  std::string data = "operation=" + operation + ", source=" + source + ", field_count=" + std::to_string(fieldCount);
  if (!fieldList.empty()) {
    data += ", fields=[" + fieldList + "]";
  }
  logVerbose("SETTINGS", "Configuration operation", data);
}

void BeaconLogger::logSystemEvent(const std::string& event, const std::string& data) {
  logBasic("SYSTEM", event, data);
}

void BeaconLogger::logSystemVerbose(const std::string& event, const std::string& data, const std::string& memoryInfo) {
  std::string fullData = data;
  if (!memoryInfo.empty()) {
    fullData += (fullData.empty() ? "" : ", ") + memoryInfo;
  }
  logVerbose("SYSTEM", event + " details", fullData);
}

void BeaconLogger::logNetworkEvent(const std::string& event, const std::string& data) {
  logBasic("NETWORK", event, data);
}

void BeaconLogger::logFsmEvent(const std::string& event, const std::string& data) {
  logBasic("FSM", event, data);
}

void BeaconLogger::logSchedulerEvent(const std::string& event, const std::string& data) {
  logBasic("SCHEDULER", event, data);
}

std::string BeaconLogger::getConfigurationSummary() const {
  std::ostringstream summary;
  summary << "Logging Configuration: default=" << logLevelToString(defaultLevel);
  
  if (!subsystemLevels.empty()) {
    summary << ", subsystems={";
    bool first = true;
    for (const auto& pair : subsystemLevels) {
      if (!first) summary << ", ";
      summary << pair.first << "=" << logLevelToString(pair.second);
      first = false;
    }
    summary << "}";
  }
  
#ifdef ESP32_PLATFORM
  summary << ", target=ESP32";
#else
  summary << ", target=HOST";
  if (fileLogging) {
    summary << ", output=FILE";
  } else {
    summary << ", output=STDERR";
  }
#endif
  
  return summary.str();
}