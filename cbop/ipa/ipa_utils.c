#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>
#include <unistd.h>
#include <errno.h>
#include "ipa.h"
#include "timing.h" // Used for the time to seconds macro

// Require placing data in user-supplied buffer

void ipa_strerr(char * buffer, size_t len) {
  // For now, no error handling
  assert(strerror_r(errno, buffer, len) == 0);
}

// Get the
void ipa_perror(const char * msg) {
  char error_buffer[512]; // holds strerror
  char master_buffer[2048]; // final thing to write out
  // Get the error string
  ipa_strerr(&error_buffer[0], sizeof(error_buffer));
  size_t tsize = snprintf(&master_buffer[0], sizeof(master_buffer), "%s: %s\n", msg, &error_buffer[0]);
  // Get

  if (write(STDERR_FILENO, &master_buffer[0], tsize) == -1) {
    // TODO log the error somewhere where dynamically allocation won't break all of the things
  }
}

#include <locale.h>
#include <math.h>

void print_ipa_stats() {
#ifdef COLLECT_STATS
  shared_data_t snapshot = *shared;
  if (!isatty(fileno(stderr))) {
    return; // Do not print stats if output is redirected
  }
  int index;
  setlocale(LC_ALL,"");
  printf("BOMALLOC stats\n");
  printf("allocations: %lu\n", snapshot.allocations);
  printf("frees: %lu\n", snapshot.frees);
  printf("sbrks: %lu\n", snapshot.sbrks);
  printf("spec sbrks: %lu\n", snapshot.spec_sbrks);
  printf("blocks: %lu\n", snapshot.total_blocks);
  printf("huge allocations: %lu\n", snapshot.huge_allocations);
  printf("header pages: %lu\n", snapshot.header_pages);
  printf("Unmappings: %lu\n",snapshot.num_unmaps);
  printf("headers / page: %lu\n", HEADERS_PER_PAGE);
  printf("Time spent allocating: %'lu ns (%'.5lf s)\n", snapshot.time_malloc, TIME_TO_SEC(snapshot.time_malloc));
  double average = ((double) snapshot.time_malloc) / snapshot.allocations;
  printf("Average time per allocation: %'.2lf ns\n", average);
  for (index = 0; index < NUM_CLASSES; index++) {
    printf("class %2d (%'10lu B) allocations: %lu\n", index, CLASS_TO_SIZE(index), snapshot.allocs_per_class[index]);
  }
  printf("Total managed memory:\n\t%'lu B\n\t%'.0lf pages\n\t%'lu frames\n\t%'.2lf GB\n\t%'.2lf TB\n",
          (size_t) snapshot.total_alloc,
          ceil(((double) snapshot.total_alloc) / PAGE_SIZE),
          snapshot.total_frames,
          ((double) snapshot.total_alloc) / (1024L * 1024 * 1024),
#if __WORDSIZE == 64
          ((double) shared->total_alloc) / (1024L * 1024 * 1024 * 1024)
#else
          (double) 0
#endif
        );
#else
  printf("BOMALLOC not configured to collect statistics\n");
#endif
}

extern int getuniqueid(void);

void ipa_teardown() {
  char path[2048];
  int written;
  bool no_errors = true;
  // ensure the directory is present
  for (int i = 1; i <= shared->next_name; i++) {
    written = snprintf(&path[0], sizeof(path), "%s%d/%d", "/tmp/bop/", getuniqueid(), i);
    if (written > sizeof(path) || written < 0) {
      ipa_perror("Unable to write directory name for cleanup");
      no_errors = false;
      continue;
    }
    if (remove(path)) {
      ipa_perror("Unable to remove file");
      no_errors = false;
    }
  }
  if (no_errors && shared->next_name != 0) {
    written = snprintf(&path[0], sizeof(path), "%s%d/", "/tmp/bop/", getuniqueid());
    if (written > sizeof(path) || written < 0) {
      ipa_perror("Unable to write directory name for cleanup");
    }
    if (remove(path)) {
      ipa_perror("Unable to remove directory");
    }
  }
}
