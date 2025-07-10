#include "Settings.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char *defaultSettingsJson = R"({
  "call": "N0CALL",
  "loc": "AA00aa",
  "pwr": 10,
  "txPct": 100,
  "txIntervalMinutes": 4,
  "bandMode": "sequential",
  "wifiMode": "sta",
  "ssid": "",
  "pwd": "",
  "timezone": "UTC",
  "autoTimezone": true,
  "bands": {
    "160m": {"en": 0, "freq": 1836600, "sched": 16777215},
    "80m": {"en": 0, "freq": 3568600, "sched": 16777215},
    "60m": {"en": 0, "freq": 5287200, "sched": 16777215},
    "40m": {"en": 0, "freq": 7038600, "sched": 16777215},
    "30m": {"en": 0, "freq": 10138700, "sched": 16777215},
    "20m": {"en": 1, "freq": 14095600, "sched": 16777215},
    "17m": {"en": 0, "freq": 18104600, "sched": 16777215},
    "15m": {"en": 0, "freq": 21094600, "sched": 16777215},
    "12m": {"en": 0, "freq": 24924600, "sched": 16777215},
    "10m": {"en": 0, "freq": 28124600, "sched": 16777215},
    "6m": {"en": 0, "freq": 50293000, "sched": 16777215},
    "2m": {"en": 0, "freq": 144488500, "sched": 16777215}
  }
})";

Settings::Settings()
  : defaults(nullptr), user(nullptr) {
  defaults = cJSON_Parse(defaultSettingsJson);
  if (!defaults) {
    printf("Default JSON is invalid!\n");
    defaults = cJSON_CreateObject();
  }
  loadFromFile();
  mergeDefaults();
}

Settings::~Settings() {
  if (defaults) cJSON_Delete((cJSON *)defaults);
  if (user) cJSON_Delete((cJSON *)user);
}

void Settings::mergeDefaults() {
  if (!user || !defaults) return;
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, (cJSON *)defaults) {
    const char *key = it->string;
    cJSON *userItem = cJSON_GetObjectItem((cJSON *)user, key);
    if (!userItem || userItem->type != it->type) {
      if (userItem) cJSON_DeleteItemFromObject((cJSON *)user, key);
      cJSON_AddItemToObject((cJSON *)user, key, cJSON_Duplicate(it, 1));
    }
  }
}

void Settings::loadFromFile() {
  FILE *f = fopen("settings.json", "r");
  if (!f) {
    printf("loadFromFile: settings.json not found - creating empty object\n");
    user = cJSON_CreateObject();
    return;
  }

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (len <= 0) {
    printf("loadFromFile: settings.json empty - creating empty object\n");
    fclose(f);
    user = cJSON_CreateObject();
    return;
  }

  char *buf = (char *)malloc(len + 1);
  if (!buf) {
    printf("loadFromFile: malloc failed - creating empty object\n");
    fclose(f);
    user = cJSON_CreateObject();
    return;
  }

  size_t bytesRead = fread(buf, 1, len, f);
  buf[bytesRead] = '\0';
  fclose(f);

  printf("LOAD Settings JSON: %s\n", buf);

  if (bytesRead > 0 && buf[0] != '\0') {
    user = cJSON_Parse(buf);
    if (!user) {
      printf("Invalid user JSON in file, using empty object\n");
      user = cJSON_CreateObject();
    }
  } else {
    printf("loadFromFile: JSON parse failed - creating empty object\n");
    user = cJSON_CreateObject();
  }

  free(buf);
}

bool Settings::store() {
  char *json = toJsonString();
  if (!json) return false;

  FILE *f = fopen("settings.json", "w");
  if (!f) {
    printf("storeToFile: fopen failed\n");
    free(json);
    return false;
  }

  printf("STORE Settings JSON: %s\n", json);

  size_t len = strlen(json);
  size_t written = fwrite(json, 1, len, f);
  fclose(f);
  free(json);

  return written == len;
}

int Settings::getInt(const char *key, int defaultValue) const {
  cJSON *it = cJSON_GetObjectItem((cJSON *)user, key);
  if (it && cJSON_IsNumber(it)) return it->valueint;
  it = cJSON_GetObjectItem((cJSON *)defaults, key);
  if (it && cJSON_IsNumber(it)) return it->valueint;
  return defaultValue;
}

float Settings::getFloat(const char *key, float defaultValue) const {
  cJSON *it = cJSON_GetObjectItem((cJSON *)user, key);
  if (it && cJSON_IsNumber(it)) return (float)it->valuedouble;
  it = cJSON_GetObjectItem((cJSON *)defaults, key);
  if (it && cJSON_IsNumber(it)) return (float)it->valuedouble;
  return defaultValue;
}

const char *Settings::getString(const char *key, const char *defaultValue) const {
  cJSON *it = cJSON_GetObjectItem((cJSON *)user, key);
  if (it && cJSON_IsString(it) && it->valuestring) {
    return it->valuestring;
  }
  it = cJSON_GetObjectItem((cJSON *)defaults, key);
  if (it && cJSON_IsString(it) && it->valuestring) {
    return it->valuestring;
  }
  return defaultValue;
}

void Settings::setInt(const char *key, int value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem((cJSON *)user, key);
  if (item) {
    cJSON_ReplaceItemInObject((cJSON *)user, key, cJSON_CreateNumber(value));
  } else {
    cJSON_AddNumberToObject((cJSON *)user, key, value);
  }
}

void Settings::setFloat(const char *key, float value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem((cJSON *)user, key);
  if (item) {
    cJSON_ReplaceItemInObject((cJSON *)user, key, cJSON_CreateNumber(value));
  } else {
    cJSON_AddNumberToObject((cJSON *)user, key, value);
  }
}

void Settings::setString(const char *key, const char *value) {
  if (!user) return;
  cJSON *item = cJSON_GetObjectItem((cJSON *)user, key);
  if (item) {
    cJSON_ReplaceItemInObject((cJSON *)user, key, cJSON_CreateString(value));
  } else {
    cJSON_AddStringToObject((cJSON *)user, key, value);
  }
}

char *Settings::toJsonString() const {
  cJSON *merged = cJSON_Duplicate((cJSON *)defaults, 1);
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, (cJSON *)user) {
    cJSON *inMerged = cJSON_GetObjectItem(merged, it->string);
    if (inMerged && inMerged->type == it->type) {
      cJSON_ReplaceItemInObject(merged, it->string, cJSON_Duplicate(it, 1));
    } else if (!inMerged) {
      cJSON_AddItemToObject(merged, it->string, cJSON_Duplicate(it, 1));
    }
  }
  char *json = cJSON_PrintUnformatted(merged);
  cJSON_Delete(merged);
  return json;
}

bool Settings::fromJsonString(const char *jsonString) {
  if (!user) return false;
  
  printf("fromJsonString: Parsing JSON: %s\n", jsonString);
  
  cJSON *parsed = cJSON_Parse(jsonString);
  if (!parsed) {
    printf("fromJsonString: Failed to parse JSON\n");
    return false;
  }
  
  // Clear existing user settings to start fresh
  cJSON_Delete((cJSON *)user);
  user = cJSON_CreateObject();
  
  cJSON *it = nullptr;
  cJSON_ArrayForEach(it, parsed) {
    const char *key = it->string;
    
    if (cJSON_IsString(it)) {
      setString(key, it->valuestring);
    } else if (cJSON_IsNumber(it)) {
      setFloat(key, (float)it->valuedouble);
    } else if (cJSON_IsObject(it)) {
      // Handle nested objects (like bands)
      cJSON_AddItemToObject((cJSON *)user, key, cJSON_Duplicate(it, 1));
    }
  }
  
  cJSON_Delete(parsed);
  printf("fromJsonString: Completed successfully\n");
  return true;
}