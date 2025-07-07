#pragma once

#include <functional>

// Forward declarations
class Scheduler;

class WebServerIntf {
public:
  virtual ~WebServerIntf() {}

  virtual void start() = 0;
  virtual void stop() = 0;

  // Register callback for settings changes (may be ignored if not needed)
  virtual void setSettingsChangedCallback(const std::function<void()> &cb) = 0;
  
  // Set scheduler reference for countdown API (may be ignored if not needed)
  virtual void setScheduler(Scheduler* scheduler) = 0;
};