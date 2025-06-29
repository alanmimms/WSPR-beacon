#include "FileSystemIntf.h"
#include <cstdio>
#include <cstring>

class FileSystem : public FileSystemIntf {
public:
  FileSystem() {}
  ~FileSystem() override {}

  bool mount(const char *path) override { return true; }
  bool unmount(const char *path) override { return true; }
  FILE *open(const char *path, const char *mode) override { return fopen(path, mode); }
  size_t read(void *ptr, size_t size, size_t nmemb, FILE *stream) override { return fread(ptr, size, nmemb, stream); }
  size_t write(const void *ptr, size_t size, size_t nmemb, FILE *stream) override { return fwrite(ptr, size, nmemb, stream); }
  int close(FILE *stream) override { return fclose(stream); }
  int remove(const char *path) override { return std::remove(path); }
};