#pragma once

#include <cstddef> // For size_t

// The number of settings managed by the class.
#define SETTINGS_COUNT 8

// A struct to hold all information about a single setting.
// The value is a dynamically allocated, null-terminated string.
struct Setting {
    const char* key; // The key used in NVS and HTML forms
    char* value;     // A malloc'd, null-terminated string
};

class Settings {
public:
    Settings();
    ~Settings();

    void load();
    void save();

    // Generic methods to get and set values by their string key
    const char* getValue(const char* key) const;
    void setValue(const char* key, const char* value);

    // The settings array is public to allow iteration by other classes.
    Setting settings[SETTINGS_COUNT];
    const int numSettings = SETTINGS_COUNT;
};
