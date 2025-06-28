#pragma once

#include "LoggerIntf.h"

class Logger : public LoggerIntf {
public:
  Logger();
  ~Logger() override;

  void logInfo(const char *msg) override;
  void logWarn(const char *msg) override;
  void logError(const char *msg) override;
  void logDebug(const char *msg) override;
};