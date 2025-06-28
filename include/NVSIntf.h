#pragma once

class NVSIntf {
public:
  virtual ~NVSIntf() {}

  virtual bool init() = 0;
  virtual bool readU32(const char *key, unsigned int *value) = 0;
  virtual bool writeU32(const char *key, unsigned int value) = 0;
  virtual bool readI32(const char *key, int *value) = 0;
  virtual bool writeI32(const char *key, int value) = 0;
  virtual bool readStr(const char *key, char *value, unsigned int maxLen) = 0;
  virtual bool writeStr(const char *key, const char *value) = 0;
  virtual bool eraseKey(const char *key) = 0;
  virtual bool eraseAll() = 0;
  virtual void commit() = 0;
};