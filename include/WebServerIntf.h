#pragma once

#include <functional>
#include <cstdint>

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
  
  // Update beacon state for status display (may be ignored if not needed)
  virtual void updateBeaconState(const char* networkState, const char* transmissionState, const char* band, uint32_t frequency) = 0;
};