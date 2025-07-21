#ifndef SETTINGS_BASE_H
#define SETTINGS_BASE_H

#include "SettingsIntf.h"
#include "cJSON.h"
#include <string>

/**
 * Base class for Settings implementations that provides common JSON handling logic.
 * Platform-specific implementations only need to implement the storage methods.
 */
class SettingsBase : public SettingsIntf {
public:
    SettingsBase();
    virtual ~SettingsBase();
    
    // Initialize settings - must be called after construction
    void initialize();

    // SettingsIntf implementation
    int getInt(const char* key, int defaultValue) const override;
    float getFloat(const char* key, float defaultValue) const override;
    const char* getString(const char* key, const char* defaultValue) const override;
    
    void setInt(const char* key, int value) override;
    void setFloat(const char* key, float value) override;
    void setString(const char* key, const char* value) override;
    
    char* toJsonString() const override;
    bool fromJsonString(const char* jsonString) override;
    
    // Platform-specific methods to be implemented by subclasses
    virtual bool loadFromStorage() = 0;
    virtual bool saveToStorage() = 0;
    
    // Common implementation of store() that calls saveToStorage()
    bool store() override { return saveToStorage(); }

protected:
    // Common initialization
    void initializeDefaults();
    void mergeDefaults();
    
    // Platform-specific logging to be implemented by subclasses
    virtual void logInfo(const char* format, ...) = 0;
    virtual void logError(const char* format, ...) = 0;
    
    // JSON objects
    cJSON* defaults;
    cJSON* user;
    
    // Default configuration string - shared by all platforms
    static const char* DEFAULT_JSON;
};

#endif // SETTINGS_BASE_H