#include "FileSystem.h"
#include <esp_spiffs.h>
#include <esp_log.h>

FileSystem::FileSystem() {}

FileSystem::~FileSystem() {}

bool FileSystem::mount(const char *path) {
  esp_vfs_spiffs_conf_t conf = {
    .base_path = path,
    .partition_label = "storage",
    .max_files = 10,
    .format_if_mount_failed = false
  };
  return esp_vfs_spiffs_register(&conf) == ESP_OK;
}

bool FileSystem::unmount(const char *path) {
  return esp_vfs_spiffs_unregister("storage") == ESP_OK;
}

FILE *FileSystem::open(const char *path, const char *mode) {
  return fopen(path, mode);
}

size_t FileSystem::read(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return fread(ptr, size, nmemb, stream);
}

size_t FileSystem::write(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return fwrite(ptr, size, nmemb, stream);
}

int FileSystem::close(FILE *stream) {
  return fclose(stream);
}

int FileSystem::remove(const char *path) {
  return ::remove(path);
}