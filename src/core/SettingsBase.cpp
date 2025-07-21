#include "SettingsBase.h"
#include <cstring>
#include <cstdlib>

// Default JSON configuration shared by all platforms
// Uses short property names to save space in embedded storage
// Schedule uses bit mask: 1<<hour for enabled hours (e.g., 0x3FC00 = hours 10-17)
const char* SettingsBase::DEFAULT_JSON = 
    "{"
    "\"nodeName\":\"BEACON-001\","
    "\"callsign\":\"W1AW\","
    "\"locator\":\"FN31pr\","
    "\"powerDbm\":23,"
    "\"bands\":{"
    "\"10m\":{\"freq\":28124600,\"sched\":4194048,\"en\":true},"
    "\"12m\":{\"freq\":24924600,\"sched\":0,\"en\":false},"
    "\"15m\":{\"freq\":21094600,\"sched\":16777152,\"en\":true},"
    "\"17m\":{\"freq\":18104600,\"sched\":16777215,\"en\":true},"
    "\"20m\":{\"freq\":14095600,\"sched\":16777215,\"en\":true},"
    "\"30m\":{\"freq\":10138700,\"sched\":12582975,\"en\":true},"
    "\"40m\":{\"freq\":7038600,\"sched\":15728895,\"en\":true},"
    "\"80m\":{\"freq\":3568600,\"sched\":15728895,\"en\":true},"
    "\"160m\":{\"freq\":1836600,\"sched\":0,\"en\":false},"
    "\"60m\":{\"freq\":5287200,\"sched\":0,\"en\":false},"
    "\"6m\":{\"freq\":50293100,\"sched\":0,\"en\":false},"
    "\"2m\":{\"freq\":144488500,\"sched\":0,\"en\":false}"
    "},"
    "\"wifi\":{\"ssid\":\"\",\"password\":\"\"},"
    "\"crystal\":{\"freqHz\":26000000,\"correctionPPM\":0}"
    "}";

SettingsBase::SettingsBase() : defaults(nullptr), user(nullptr) {
    // Don't call virtual functions in constructor
}

SettingsBase::~SettingsBase() {
    if (defaults) {
        cJSON_Delete(defaults);
        defaults = nullptr;
    }
    if (user) {
        cJSON_Delete(user);
        user = nullptr;
    }
}

void SettingsBase::initialize() {
    // Parse default JSON
    defaults = cJSON_Parse(DEFAULT_JSON);
    if (!defaults) {
        logError("Failed to parse default settings JSON");
        defaults = cJSON_CreateObject();
    }
    
    // Create empty user settings
    user = cJSON_CreateObject();
    
    // Load from platform-specific storage
    if (!loadFromStorage()) {
        logInfo("No stored settings found, using defaults");
    }
    
    // Ensure all defaults are present
    mergeDefaults();
}

void SettingsBase::mergeDefaults() {
    if (!defaults || !user) return;
    
    cJSON* item = defaults->child;
    while (item) {
        if (!cJSON_GetObjectItem(user, item->string)) {
            cJSON* duplicate = cJSON_Duplicate(item, 1);
            if (duplicate) {
                cJSON_AddItemToObject(user, item->string, duplicate);
            }
        }
        item = item->next;
    }
}

// Getter implementations
int SettingsBase::getInt(const char* key, int defaultValue) const {
    if (!key) return defaultValue;
    
    // Check user settings first
    cJSON* item = cJSON_GetObjectItem(user, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    
    // Fall back to defaults
    item = cJSON_GetObjectItem(defaults, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    
    return defaultValue;
}

float SettingsBase::getFloat(const char* key, float defaultValue) const {
    if (!key) return defaultValue;
    
    // Check user settings first
    cJSON* item = cJSON_GetObjectItem(user, key);
    if (item && cJSON_IsNumber(item)) {
        return (float)item->valuedouble;
    }
    
    // Fall back to defaults
    item = cJSON_GetObjectItem(defaults, key);
    if (item && cJSON_IsNumber(item)) {
        return (float)item->valuedouble;
    }
    
    return defaultValue;
}

const char* SettingsBase::getString(const char* key, const char* defaultValue) const {
    if (!key) return defaultValue;
    
    // Check user settings first
    cJSON* item = cJSON_GetObjectItem(user, key);
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }
    
    // Fall back to defaults
    item = cJSON_GetObjectItem(defaults, key);
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }
    
    return defaultValue;
}

// Setter implementations
void SettingsBase::setInt(const char* key, int value) {
    if (!key || !user) return;
    
    cJSON* existing = cJSON_GetObjectItem(user, key);
    if (existing) {
        cJSON_SetIntValue(existing, value);
    } else {
        cJSON_AddNumberToObject(user, key, value);
    }
}

void SettingsBase::setFloat(const char* key, float value) {
    if (!key || !user) return;
    
    cJSON* existing = cJSON_GetObjectItem(user, key);
    if (existing) {
        cJSON_SetNumberValue(existing, value);
    } else {
        cJSON_AddNumberToObject(user, key, value);
    }
}

void SettingsBase::setString(const char* key, const char* value) {
    if (!key || !value || !user) return;
    
    cJSON* existing = cJSON_GetObjectItem(user, key);
    if (existing) {
        if (cJSON_IsString(existing)) {
            cJSON_SetValuestring(existing, value);
        } else {
            cJSON_DeleteItemFromObject(user, key);
            cJSON_AddStringToObject(user, key, value);
        }
    } else {
        cJSON_AddStringToObject(user, key, value);
    }
}

// JSON conversion methods
char* SettingsBase::toJsonString() const {
    if (!defaults || !user) return nullptr;
    
    // Create a merged copy
    cJSON* merged = cJSON_Duplicate(defaults, 1);
    if (!merged) return nullptr;
    
    // Overlay user settings
    cJSON* item = user->child;
    while (item) {
        cJSON* existing = cJSON_GetObjectItem(merged, item->string);
        if (existing) {
            cJSON_DeleteItemFromObject(merged, item->string);
        }
        cJSON* duplicate = cJSON_Duplicate(item, 1);
        if (duplicate) {
            cJSON_AddItemToObject(merged, item->string, duplicate);
        }
        item = item->next;
    }
    
    char* result = cJSON_Print(merged);
    cJSON_Delete(merged);
    return result;
}

bool SettingsBase::fromJsonString(const char* jsonString) {
    if (!jsonString) return false;
    
    cJSON* parsed = cJSON_Parse(jsonString);
    if (!parsed) {
        logError("Failed to parse settings JSON");
        return false;
    }
    
    // Clear existing user settings
    cJSON_Delete(user);
    user = cJSON_CreateObject();
    
    // Copy all items to user settings
    cJSON* item = parsed->child;
    while (item) {
        cJSON* duplicate = cJSON_Duplicate(item, 1);
        if (duplicate) {
            cJSON_AddItemToObject(user, item->string, duplicate);
        }
        item = item->next;
    }
    
    cJSON_Delete(parsed);
    
    // Ensure defaults are still present
    mergeDefaults();
    
    return true;
}