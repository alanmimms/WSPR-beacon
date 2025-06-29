#pragma once

#include "NVSIntf.h"
#include <nvs.h>

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
  nvs_handle_t handle;
  bool opened;
};