#include "Settings.h"
#include <cstdio>
#include <cstring>

Settings::Settings() {
  userJson = {
    {"call", "N0CALL"},
    {"loc", "AA00aa"},
    {"pwr", 10},
    {"txIntervalMinutes", 4}
  };
}

Settings::~Settings() {}

int Settings::getInt(const char *key, int defaultValue) const {
  if (userJson.contains(key) && userJson[key].is_number_integer())
    return userJson[key].get<int>();
  return defaultValue;
}

float Settings::getFloat(const char *key, float defaultValue) const {
  if (userJson.contains(key) && userJson[key].is_number())
    return userJson[key].get<float>();
  return defaultValue;
}

const char *Settings::getString(const char *key, const char *defaultValue) const {
  if (userJson.contains(key) && userJson[key].is_string()) {
    tempString = userJson[key].get<std::string>();
    return tempString.c_str();
  }
  return defaultValue;
}

void Settings::setInt(const char *key, int value) {
  userJson[key] = value;
}

void Settings::setFloat(const char *key, float value) {
  userJson[key] = value;
}

void Settings::setString(const char *key, const char *value) {
  userJson[key] = value;
}

bool Settings::store() {
  FILE *f = fopen("settings.json", "w");
  if (!f) return false;
  std::string s = userJson.dump();
  fwrite(s.c_str(), 1, s.length(), f);
  fclose(f);
  return true;
}

char *Settings::toJsonString() const {
  std::string s = userJson.dump();
  char *out = (char *)malloc(s.size() + 1);
  strcpy(out, s.c_str());
  return out;
}

bool Settings::fromJsonString(const char *jsonString) {
  try {
    userJson = nlohmann::json::parse(jsonString);
    return true;
  } catch (...) {
    return false;
  }
}