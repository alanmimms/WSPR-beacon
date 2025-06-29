#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Abstract interface for file system and file access.
 * Allows mounting, opening, reading, writing, stat, and closing files.
 * Implemented for both POSIX (host) and ESP-IDF (target).
 */
class FileSystemIntf {
public:
  virtual ~FileSystemIntf() {}

  // Mount the filesystem. Returns true on success.
  virtual bool mount() = 0;

  // Unmount the filesystem.
  virtual void unmount() = 0;

  // Open a file. Returns file handle (opaque pointer) or nullptr on error.
  virtual void *open(const char *path, const char *mode) = 0;

  // Close a previously opened file handle.
  virtual void close(void *file) = 0;

  // Read up to 'len' bytes from file into buffer. Returns number of bytes read, or -1 on error.
  virtual int read(void *file, void *buffer, size_t len) = 0;

  // Write up to 'len' bytes from buffer to file. Returns number of bytes written, or -1 on error.
  virtual int write(void *file, const void *buffer, size_t len) = 0;

  // Set the file position. Returns true on success.
  virtual bool seek(void *file, int64_t offset, int whence) = 0;

  // Get the file size. Returns true on success and sets result.
  virtual bool size(const char *path, uint64_t &result) = 0;

  // Stat a file (existence, regular file, etc). Returns true if file exists and is regular.
  virtual bool stat(const char *path) = 0;

  // Remove a file. Returns true on success.
  virtual bool remove(const char *path) = 0;
};