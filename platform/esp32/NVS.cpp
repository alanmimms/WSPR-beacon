#include "NVS.h"
#include <cstring>
#include <nvs_flash.h>
#include <esp_log.h>

#define NVS_NAMESPACE "app"

NVS::NVS() : handle(0), opened(false) {
  nvs_flash_init();
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) opened = true;
}

NVS::~NVS() {
  if (opened) nvs_close(handle);
}

bool NVS::init() {
  return opened;
}

bool NVS::readU32(const char *key, unsigned int *value) {
  return opened && nvs_get_u32(handle, key, value) == ESP_OK;
}

bool NVS::writeU32(const char *key, unsigned int value) {
  return opened && nvs_set_u32(handle, key, value) == ESP_OK;
}

bool NVS::readI32(const char *key, int *value) {
  return opened && nvs_get_i32(handle, key, value) == ESP_OK;
}

bool NVS::writeI32(const char *key, int value) {
  return opened && nvs_set_i32(handle, key, value) == ESP_OK;
}

bool NVS::readStr(const char *key, char *value, unsigned int maxLen) {
  size_t len = maxLen;
  return opened && nvs_get_str(handle, key, value, &len) == ESP_OK;
}

bool NVS::writeStr(const char *key, const char *value) {
  return opened && nvs_set_str(handle, key, value) == ESP_OK;
}

bool NVS::eraseKey(const char *key) {
  return opened && nvs_erase_key(handle, key) == ESP_OK;
}

bool NVS::eraseAll() {
  return opened && nvs_erase_all(handle) == ESP_OK;
}

void NVS::commit() {
  if (opened) nvs_commit(handle);
}