#pragma once

class LoggerIntf {
public:
  virtual ~LoggerIntf() {}

  virtual void logInfo(const char *msg) = 0;
  virtual void logWarn(const char *msg) = 0;
  virtual void logError(const char *msg) = 0;
  virtual void logDebug(const char *msg) = 0;
};