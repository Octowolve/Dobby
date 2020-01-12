#include <stdlib.h> /* getenv */
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#include <iostream>
#include <fstream>

#include <set>

#include <unordered_map>

#include "./dynamic_loader_monitor.h"

std::unordered_map<int, const char *> posix_file_descriptors;

int (*orig_open)(const char *pathname, int flags, ...);
int fake_open(const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = (mode_t)va_arg(args, int);
    va_end(args);
  }

  int result = orig_open(pathname, flags, mode);
  if (result != -1) {
    char *traced_filename = (char *)malloc(128);
    // FIXME: strncpy
    strcpy(traced_filename, pathname);
    std::cout << "[-] trace open handle: " << pathname << std::endl;
    posix_file_descriptors.insert(std::make_pair(result, (const char *)traced_filename));
  }
  return result;
}

static const char *get_traced_filename(int fd, bool removed) {
  std::unordered_map<int, const char *>::iterator it;
  it = posix_file_descriptors.find(fd);
  if (it != posix_file_descriptors.end()) {
    if (removed)
      posix_file_descriptors.erase(it);
    return it->second;
  }
  return NULL;
}

ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t fake_read(int fd, void *buf, size_t count) {
  const char *traced_filename = get_traced_filename(fd, false);
  if (traced_filename) {
    printf("[-] read: %s, buffer: %s, size: %zu\n", traced_filename, buf, count);
  }
  return orig_read(fd, buf, count);
}

ssize_t (*orig_write)(int fd, const void *buf, size_t count);
ssize_t fake_write(int fd, const void *buf, size_t count) {
  const char *traced_filename = get_traced_filename(fd, false);
  if (traced_filename) {
    printf("[-] write: %s, buffer: %s, size: %zu\n", traced_filename, buf, count);
  }
  return fake_write(fd, buf, count);
}
int (*orig_close)(int fd);
int fake_close(int fd) {
  const char *traced_filename = get_traced_filename(fd, true);
  if (traced_filename) {
    printf("[-] dlclose: %s\n", traced_filename);
    free((void *)traced_filename);
  }
  return orig_close(fd);
}
