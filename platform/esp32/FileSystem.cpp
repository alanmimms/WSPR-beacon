#include "FileSystem.h"
#include <esp_spiffs.h>
#include <esp_log.h>
#include <sys/stat.h>
#include <unistd.h>

FileSystem::FileSystem() {}

FileSystem::~FileSystem() {}

bool FileSystem::mount() {
  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = "storage",
    .max_files = 10,
    .format_if_mount_failed = false
  };
  return esp_vfs_spiffs_register(&conf) == ESP_OK;
}

void FileSystem::unmount() {
  esp_vfs_spiffs_unregister("storage");
}

void *FileSystem::open(const char *path, const char *mode) {
  return fopen(path, mode);
}

void FileSystem::close(void *file) {
  if (file) {
    fclose(static_cast<FILE*>(file));
  }
}

int FileSystem::read(void *file, void *buffer, size_t len) {
  if (!file || !buffer) return -1;
  size_t result = fread(buffer, 1, len, static_cast<FILE*>(file));
  return ferror(static_cast<FILE*>(file)) ? -1 : static_cast<int>(result);
}

int FileSystem::write(void *file, const void *buffer, size_t len) {
  if (!file || !buffer) return -1;
  size_t result = fwrite(buffer, 1, len, static_cast<FILE*>(file));
  return ferror(static_cast<FILE*>(file)) ? -1 : static_cast<int>(result);
}

bool FileSystem::seek(void *file, int64_t offset, int whence) {
  if (!file) return false;
  return fseek(static_cast<FILE*>(file), offset, whence) == 0;
}

bool FileSystem::size(const char *path, uint64_t &result) {
  struct stat st;
  if (::stat(path, &st) == 0) {
    result = st.st_size;
    return true;
  }
  return false;
}

bool FileSystem::stat(const char *path) {
  struct stat st;
  return ::stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool FileSystem::remove(const char *path) {
  return ::remove(path) == 0;
}