#pragma once

class SettingsIntf {
public:
  virtual ~SettingsIntf() {}

  virtual int getInt(const char *key, int defaultValue = 0) const = 0;
  virtual float getFloat(const char *key, float defaultValue = 0.0f) const = 0;
  virtual const char *getString(const char *key, const char *defaultValue = "") const = 0;

  virtual void setInt(const char *key, int value) = 0;
  virtual void setFloat(const char *key, float value) = 0;
  virtual void setString(const char *key, const char *value) = 0;

  virtual bool store() = 0;

  // Return a malloc'd JSON string representing the settings (caller free()s)
  virtual char *toJsonString() const = 0;

  // Parse a JSON string and update settings, returns true on success
  virtual bool fromJsonString(const char *jsonString) = 0;
};