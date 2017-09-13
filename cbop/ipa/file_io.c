#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <error.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <signal.h>

#include "ipa.h"
#include "file_io.h"
#include "ipa_utils.h"
#include "ipa_hooks.h"

static inline int mkdir_ne(char * path, int flags) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
    return mkdir(path, flags);
  }
  return 0;
}

size_t get_size_fd(int fd) {
  struct stat st;
  if (fstat(fd, &st) == -1) {
    ipa_perror("Unable to get size of file");
  }
  return st.st_size;
}

// Recursively make directories
// Based on:
// http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html
static int rmkdir(char *dir) {
  char tmp[256];
  char *p = NULL;
  size_t len;
  errno = 0;
  snprintf(tmp, sizeof(tmp),"%s",dir);
  len = strlen(tmp);
  if(tmp[len - 1] == '/')
          tmp[len - 1] = 0;
  for(p = tmp + 1; *p; p++) {
    if(*p == '/') {
        *p = 0;
        if (mkdir_ne(tmp, S_IRWXU)) {
          return -1;
        }
        *p = '/';
    }
  }
  return mkdir_ne(tmp, S_IRWXU);
}

size_t get_size_name(unsigned name) {
  char path[2048]; // 2 kB of path -- more than enough
  int written;
  written = snprintf(&path[0], sizeof(path), "%s%d/%u", "/tmp/bop/", getuniqueid(), name);
  if (written > sizeof(path) || written < 0) {
    ipa_perror("Unable to write the output path");
    abort();
  }
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    ipa_perror("Unable to open existing file");
    abort();
  }
  size_t size = get_size_fd(fd);
  if (close(fd)) {
    ipa_perror("Unable to close fd used for temp size pole");
  }
  return size;
}
/**
 * Open a file descriptor NAMED file_no for later use by mmap
 * @param  file_no [description]
 * @return         [description]
 */
static inline int mmap_fd_bool(unsigned file_no, size_t size, bool no_fd) {
  if (no_fd) {
    return -1;
  }
  char path[2048]; // 2 kB of path -- more than enough
  int written;
  // ensure the directory is present
  written = snprintf(&path[0], sizeof(path), "%s%d/", "/tmp/bop/", getuniqueid());
  if (written > sizeof(path) || written < 0) {
    ipa_perror("Unable to write directory name");
    abort();
  }

  if (rmkdir(&path[0]) != 0 && errno != EEXIST) {
    ipa_perror("Unable to make the directory");
    abort();
  }
  // now create the file
  written = snprintf(&path[0], sizeof(path), "%s%d/%u", "/tmp/bop/", getuniqueid(), file_no);
  if (written > sizeof(path) || written < 0) {
    ipa_perror("Unable to write the output path");
    abort();
  }

  int fd = open(path, O_RDWR | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  if (fd == -1) {
    ipa_perror("Unable to create the file.");
    abort();
  }
  if (get_size_fd(fd) < size) {
    // need to truncate (grow -- poor naming) the file
    if (ftruncate(fd, size) < 0) {
      ipa_perror("BOMALLOC_MMAP: Unable to truncate/grow file");
      abort();
    }
  }
  return fd;
}

int mmap_existing_fd(unsigned name) {
  return mmap_fd_bool(name, get_size_name(name), false);
}



int mmap_fd(unsigned name, size_t size) {
  return mmap_fd_bool(name, size, false);
}
