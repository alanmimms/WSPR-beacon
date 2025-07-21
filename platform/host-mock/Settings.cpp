#include "Settings.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

const char* Settings::SETTINGS_FILE = "settings.json";

Settings::Settings() : SettingsBase() {
    // Initialize after construction to avoid calling virtual functions in constructor
    initialize();
}

Settings::~Settings() {
    // Base class destructor handles cleanup
}

bool Settings::loadFromStorage() {
    return loadFromFile();
}

bool Settings::saveToStorage() {
    return saveToFile();
}

bool Settings::loadFromFile() {
    FILE* file = fopen(SETTINGS_FILE, "r");
    if (!file) {
        printf("[Settings] No settings file found at %s\n", SETTINGS_FILE);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(file);
        printf("[Settings] Settings file is empty\n");
        return false;
    }

    // Read file content
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        fclose(file);
        printf("[Settings] Failed to allocate memory for settings\n");
        return false;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        free(buffer);
        printf("[Settings] Failed to read settings file\n");
        return false;
    }

    buffer[fileSize] = '\0';
    bool success = fromJsonString(buffer);
    free(buffer);

    if (success) {
        printf("[Settings] Settings loaded from %s\n", SETTINGS_FILE);
    } else {
        printf("[Settings] Failed to parse settings from file\n");
    }

    return success;
}

bool Settings::saveToFile() {
    char* jsonStr = toJsonString();
    if (!jsonStr) {
        printf("[Settings] Failed to serialize settings\n");
        return false;
    }

    FILE* file = fopen(SETTINGS_FILE, "w");
    if (!file) {
        free(jsonStr);
        printf("[Settings] Failed to open %s for writing\n", SETTINGS_FILE);
        return false;
    }

    size_t length = strlen(jsonStr);
    size_t written = fwrite(jsonStr, 1, length, file);
    fclose(file);
    free(jsonStr);

    if (written != length) {
        printf("[Settings] Failed to write settings to file\n");
        return false;
    }

    printf("[Settings] Settings saved to %s\n", SETTINGS_FILE);
    return true;
}

void Settings::logInfo(const char* format, ...) {
    printf("[Settings] ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void Settings::logError(const char* format, ...) {
    fprintf(stderr, "[Settings] ERROR: ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}