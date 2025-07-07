#pragma once

#include "FileSystemIntf.h"
#include <stdio.h>

class FileSystem : public FileSystemIntf {
public:
  FileSystem();
  ~FileSystem() override;

  bool mount() override;
  void unmount() override;
  void *open(const char *path, const char *mode) override;
  void close(void *file) override;
  int read(void *file, void *buffer, size_t len) override;
  int write(void *file, const void *buffer, size_t len) override;
  bool seek(void *file, int64_t offset, int whence) override;
  bool size(const char *path, uint64_t &result) override;
  bool stat(const char *path) override;
  bool remove(const char *path) override;
};