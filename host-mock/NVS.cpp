#include "NVS.h"
#include <cstdio>
#include <cstring>

NVS::NVS() : entryCount(0) {
  for (int i = 0; i < maxKeys; i++) {
    entries[i].key[0] = 0;
  }
}

NVS::~NVS() {}

bool NVS::init() {
  printf("[NVSHostMock] init called\n");
  return true;
}

int NVS::findKey(const char *key) const {
  for (int i = 0; i < entryCount; i++) {
    if (strcmp(entries[i].key, key) == 0) {
      return i;
    }
  }
  return -1;
}

int NVS::allocKey(const char *key, Entry::type type) {
  int idx = findKey(key);
  if (idx >= 0) return idx;
  if (entryCount >= maxKeys) return -1;
  strncpy(entries[entryCount].key, key, sizeof(entries[entryCount].key) - 1);
  entries[entryCount].key[sizeof(entries[entryCount].key) - 1] = 0;
  entries[entryCount].type = type;
  return entryCount++;
}

bool NVS::readU32(const char *key, unsigned int *value) {
  int idx = findKey(key);
  if (idx < 0 || entries[idx].type != Entry::TypeU32) return false;
  *value = entries[idx].value.u32;
  printf("[NVSHostMock] readU32 key=%s value=%u\n", key, *value);
  return true;
}

bool NVS::writeU32(const char *key, unsigned int value) {
  int idx = allocKey(key, Entry::TypeU32);
  if (idx < 0) return false;
  entries[idx].value.u32 = value;
  entries[idx].type = Entry::TypeU32;
  printf("[NVSHostMock] writeU32 key=%s value=%u\n", key, value);
  return true;
}

bool NVS::readI32(const char *key, int *value) {
  int idx = findKey(key);
  if (idx < 0 || entries[idx].type != Entry::TypeI32) return false;
  *value = entries[idx].value.i32;
  printf("[NVSHostMock] readI32 key=%s value=%d\n", key, *value);
  return true;
}

bool NVS::writeI32(const char *key, int value) {
  int idx = allocKey(key, Entry::TypeI32);
  if (idx < 0) return false;
  entries[idx].value.i32 = value;
  entries[idx].type = Entry::TypeI32;
  printf("[NVSHostMock] writeI32 key=%s value=%d\n", key, value);
  return true;
}

bool NVS::readStr(const char *key, char *value, unsigned int maxLen) {
  int idx = findKey(key);
  if (idx < 0 || entries[idx].type != Entry::TypeStr) return false;
  strncpy(value, entries[idx].value.str, maxLen - 1);
  value[maxLen - 1] = 0;
  printf("[NVSHostMock] readStr key=%s value=%s\n", key, value);
  return true;
}

bool NVS::writeStr(const char *key, const char *value) {
  int idx = allocKey(key, Entry::TypeStr);
  if (idx < 0) return false;
  strncpy(entries[idx].value.str, value, sizeof(entries[idx].value.str) - 1);
  entries[idx].value.str[sizeof(entries[idx].value.str) - 1] = 0;
  entries[idx].type = Entry::TypeStr;
  printf("[NVSHostMock] writeStr key=%s value=%s\n", key, value);
  return true;
}

bool NVS::eraseKey(const char *key) {
  int idx = findKey(key);
  if (idx < 0) return false;
  for (int i = idx; i < entryCount - 1; i++) {
    entries[i] = entries[i + 1];
  }
  entryCount--;
  printf("[NVSHostMock] eraseKey key=%s\n", key);
  return true;
}

bool NVS::eraseAll() {
  entryCount = 0;
  printf("[NVSHostMock] eraseAll\n");
  return true;
}

void NVS::commit() {
  printf("[NVSHostMock] commit called\n");
}
