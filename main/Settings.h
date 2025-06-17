#pragma once

#include <cstddef>

#define SETTINGS_COUNT 10 // Increased to include new scheduling settings

struct Setting {
    const char* key;
    char* value;
};

class Settings {
public:
    Settings();
    ~Settings();

    void load();
    void save();

    const char* getValue(const char* key) const;
    void setValue(const char* key, const char* value);

    Setting settings[SETTINGS_COUNT];
    const int numSettings = SETTINGS_COUNT;
};
