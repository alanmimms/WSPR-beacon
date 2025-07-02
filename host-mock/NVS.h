#pragma once

#include "NVSIntf.h"

class NVS : public NVSIntf {
public:
  NVS();
  ~NVS() override;

  bool init() override;
  bool readU32(const char *key, unsigned int *value) override;
  bool writeU32(const char *key, unsigned int value) override;
  bool readI32(const char *key, int *value) override;
  bool writeI32(const char *key, int value) override;
  bool readStr(const char *key, char *value, unsigned int maxLen) override;
  bool writeStr(const char *key, const char *value) override;
  bool eraseKey(const char *key) override;
  bool eraseAll() override;
  void commit() override;

private:
  static const int maxKeys = 32;
  struct Entry {
    char key[32];
    enum Type { TypeU32, TypeI32, TypeStr } type;
    union {
      unsigned int u32;
      int i32;
      char str[64];
    } value;
  };
  Entry entries[maxKeys];
  int entryCount;

  int findKey(const char *key) const;
  int allocKey(const char *key, Entry::Type type);
};