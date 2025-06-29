#pragma once

#include "FileSystemIntf.h"
#include <stdio.h>

class FileSystem : public FileSystemIntf {
public:
  FileSystem();
  ~FileSystem() override;

  bool mount(const char *path) override;
  bool unmount(const char *path) override;
  FILE *open(const char *path, const char *mode) override;
  size_t read(void *ptr, size_t size, size_t nmemb, FILE *stream) override;
  size_t write(const void *ptr, size_t size, size_t nmemb, FILE *stream) override;
  int close(FILE *stream) override;
  int remove(const char *path) override;
};