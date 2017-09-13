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
#include <math.h>

#include "ipa.h"
#include "memmap.h"
#include "ipa_utils.h"
#include "file_io.h"



static void map_now(volatile ipa_page_t *);
extern shared_data_t * shared;
extern bool speculating(void);
extern void ipa_init(void);

static volatile ipa_page_t * map_info;
static bool is_mapped_bool;

header_page_t * seq_headers, * seq_headers_last;

void map_handler(int x) {
  is_mapped_bool = false;
  map_now(map_info);
}

void segv_bool_test(int x) {
  is_mapped_bool = false;
}

bool is_addr_mapped(volatile void * address) {
  is_mapped_bool = true;
  typeof(&segv_bool_test) old = signal(SIGSEGV, segv_bool_test);
  if (old == SIG_ERR) {
    ipa_perror("Unable to install signal handler");
    abort();
  }
  __sync_synchronize();
  volatile int __attribute__((__unused__)) data = *(int *) address;
  __sync_synchronize();
  signal(SIGSEGV, old);
  return is_mapped_bool;
}

bool is_mapped_segv_check(volatile ipa_page_t * prev)  {
  map_info = prev;
  is_mapped_bool = true;
  typeof(&map_handler) old = signal(SIGSEGV, map_handler);
  if (old == SIG_ERR) {
    ipa_perror("Unable to install signal handler");
    abort();
  }
  __sync_synchronize();
  volatile ipa_page_t __attribute__((__unused__)) data = *(ipa_page_t *) prev->next_page;
  __sync_synchronize();
  signal(SIGSEGV, old);
  return is_mapped_bool;
}

bool is_mapped(void * ptr) {
  ipa_page_t * aligned = (ipa_page_t *) (((intptr_t) ptr) & ~(PAGE_SIZE  - 1));
  bool b = !(msync((void *) aligned, 1, MS_ASYNC) == -1 && errno == ENOMEM);
  // errno = 0;
  return b;
}

static void map_now (volatile ipa_page_t * last_page) {
  if (last_page->next_pg_name == (unsigned) -1) {
    abort();
  }
  int fd = mmap_existing_fd(last_page->next_pg_name);
  if (fd == -1) {
    // TODO handle error
    abort();
  } else {
    size_t size = get_size_fd(fd);
    if (mmap((void *) last_page->next_page, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
      // TODO handle error
      abort();
    }
    close(fd); // think can do here
    assert(is_mapped((void *) last_page->next_page));
    if ((ipa_page_t *) last_page->next_page == last_page) {
      abort();
    }
  }
}
static inline bool full_map_check(bool faulting, volatile ipa_page_t * prev) {
  if (prev->next_pg_name == -1) {
    return true;
  } else if (!faulting) {
    return is_mapped_segv_check(prev);
  } else {
    return false;
  }
}

// Map all missing pages
// Returns the last non-null page (eg. the one with the next page filed set to null)
static inline volatile ipa_page_t * map_missing_pages_safe(bool faulting) {
  static volatile ipa_page_t * cache = NULL;
  volatile ipa_page_t * start = cache == NULL ? &shared->next_page : cache;
  volatile ipa_page_t * last_page = start;
  // These are for debugging, volatile so they can't be removed by compiler
  volatile int rounds = 0, maps = 0;
  if (start->next_page != NULL) {
    if (!full_map_check(faulting, start)) {
      maps++;
      map_now(start);
      assert(is_mapped((void *)  start->next_page));
    }
    for (last_page = start; last_page->next_page != NULL; last_page = (volatile ipa_page_t *) last_page->next_page) {
      rounds++;
      if (!full_map_check(faulting, last_page)) {
        maps++;
        map_now(last_page);
      }
      if ((ipa_page_t *) last_page->next_page == last_page) {
        // last_page->next_page = NULL;
        // last_page->next_pg_name = 0;
        // break;
        abort();
      }
    }
  }
  return last_page;
}

volatile ipa_page_t * map_missing_pages() {
  return map_missing_pages_safe(false);
}

void map_missing_pages_handler() {
  map_missing_pages_safe(true);
}

/**
 * MMap a page according to @type
 *
 * @method allocate_ipa_page
 *
 * @param  minsize             the minimal size of the allocation, including any headers
 * @param  flags               baseline flags to pass to mmap, MAP_FIXED always added,
 *                             	MAP_ANONYMOUS while not speculating
 */
static inline ipa_page_t * allocate_ipa_page(int file_no, size_t minsize, int flags) {
  ipa_page_t * allocation = NULL;
  ipa_page_t alloc = {0};
  // Reserve the resources in shared for this allocation
  int file_descriptor;
  size_t allocation_size = MAX(minsize, PAGE_SIZE);
  assert(allocation_size % PAGE_SIZE == 0);
  assert(shared != NULL);
  if (file_no == -1) {
    flags |= MAP_ANONYMOUS;
  } else {
    flags &= ~MAP_PRIVATE;
    flags |= MAP_SHARED;
  }
  volatile ipa_page_t * last_page;

  if (file_no == -1) {
    allocation = mmap(NULL, allocation_size, PROT_READ | PROT_WRITE, flags, -1, 0);
  } else {
    assert(file_no != -1);
    file_descriptor = mmap_fd(file_no, allocation_size);
    while (true) {
      last_page = map_missing_pages();
      /**
       * Let the kernel decide where to put the new page(s)
       * Tasks communicate by requiring the CAS to succeed. If it fails
       * then some other task allocated its page
       */
      allocation = mmap(NULL, allocation_size, PROT_READ | PROT_WRITE, flags, file_descriptor, 0);
      if (allocation == MAP_FAILED) {
        ipa_perror("Unable to set up mmap page");
        abort();
      }
#ifdef MANUAL_ZERO
      memset(allocation, 0, allocation_size);
#endif
      allocation->next_page =  NULL;
      allocation->next_pg_name = 0;

      alloc.next_page = (struct ipa_page_t *) allocation;
      alloc.next_pg_name = file_no;
      if (last_page == (volatile ipa_page_t *) last_page->next_page) {
        abort();
      }
      if (__sync_bool_compare_and_swap(&last_page->combined, 0, alloc.combined)) {
        assert( (ipa_page_t *) last_page->next_page == allocation);
        assert( (ipa_page_t *) allocation->next_page != last_page);
        assert( (ipa_page_t *) allocation->next_page != allocation);
        break;
      } else {
        munmap(allocation, allocation_size);
        stats_collect(&shared->num_unmaps, 1);
      }
    }
    if (file_descriptor != -1) {
      if (close(file_descriptor)) {
        ipa_perror("Unable to close file descriptor");
      }
    }
  }
  stats_collect(&shared->total_alloc, allocation_size);
  page_collect(&shared->total_frames, allocation_size);
  stats_collect(&shared->number_mmap, 1);
  return allocation;
}

header_page_t * allocate_header_page() {
  // headers are always shared -- always increment name
  const int file_no = __sync_add_and_fetch(&shared->next_name, 1);
  header_page_t * headers = (header_page_t *) allocate_ipa_page(file_no, MAX(PAGE_SIZE, sizeof(header_page_t)), MAP_SHARED);

  if (headers == (header_page_t *) -1) {
    exit(-1);
  }
  headers->next_free = 1;
  if (speculating()) {
    do {
      if (shared->header_pg == NULL) {
        headers->next_header = NULL;
      } else {
        headers->next_header = (volatile struct header_page_t *) shared->header_pg;
      }
    } while (!__sync_bool_compare_and_swap(&shared->header_pg,
                                    (header_page_t *) headers->next_header,
                                    headers));
  } else {
#ifdef SUPPORT_THREADS
    do {
      if (seq_headers == NULL) {
        headers->next_header = NULL;
      } else {
        headers->next_header = (volatile struct header_page_t *) seq_headers;
      }
    } while (!__sync_bool_compare_and_swap(&seq_headers,
                                    (header_page_t *) headers->next_header,
                                    headers));
#else
    if (seq_headers == NULL) {
      seq_headers_last = seq_headers = headers;
    } else {
      headers->next_header = (volatile struct header_page_t *) seq_headers;
      seq_headers = headers;
    }
#endif
  }
  stats_collect(&shared->header_pages, 1);
  return headers;
}

huge_block_t * allocate_large(size_t size) {
  const int file_no = !speculating() ? -1 : __sync_add_and_fetch(&shared->next_name, 1);
  // Align to a page size
  size_t alloc_size = PAGE_ALIGN((size + sizeof(huge_block_t)));
  assert(alloc_size > size);
  assert(alloc_size % PAGE_SIZE == 0);
  huge_block_t * block = (huge_block_t *) allocate_ipa_page(file_no, alloc_size, MAP_PRIVATE);
  if (block == (huge_block_t *) -1) {
    return NULL;
  }
  block->huge_block_sz = alloc_size;
  block->file_name = file_no;
  block->is_shared = file_no == -1;
  if (speculating()) {
    do {
      block->next_block = (volatile struct huge_block_t *) shared->large_block;
    } while(!__sync_bool_compare_and_swap(&shared->large_block, (huge_block_t*) block->next_block, block));
  }
  if (block == (huge_block_t *) block->next_page.next_page) {
    abort();
  }
  stats_collect(&shared->huge_allocations, 1);
  return block;
}
