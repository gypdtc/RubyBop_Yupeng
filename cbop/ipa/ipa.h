#ifndef __HAVE_ALLOC_H
#define __HAVE_ALLOC_H
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include "ipa_hooks.h"
#include "stack.h"

#if __WORDSIZE == 64
#define ALIGNMENT (8) // I think?
#elif __WORDSIZE == 32
#define ALIGNMENT (4)
#else
#error "Need 32 or 64 b wordsize"
#endif

static const size_t class_sizes[] = {
  16, 32, 48, 64, 80, 96, 112, 128, // je malloc small
  160, 192, 224, 256,
  320, 384, 448, 512,
  640, 768, 896, 1024,
  1280, 1536, 1792, 2048,
  3072, 3560, 3584,
  4096, (4096 * 2), (4096 * 3), (4096 * 4), (4096 * 5),
  (4096 * 6), (4096 * 7), (4096 * 8), (4096 * 9),
  1024 * 819, 2 * 1024 * 8192, 3 * 1024 * 8192, 4 * 1024 * 8192
};

#define NUM_CLASSES ((sizeof(class_sizes) / sizeof(size_t)))

// Header size macros
#define CLASS_TO_SIZE(x) (__size_to_class((unsigned) x)) // to actually be determined later
#define SIZE_TO_CLASS(x) (class_for_size((unsigned) x))
#define MAX_SIZE (class_sizes[NUM_CLASSES - 1])
#define MAX_REQUEST (ALIGN(MAX_SIZE - sizeof(block_t)))


#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define PAGE_ALIGN(size) (((size) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))


#define PAGE_SIZE 4096 //default Linux


static inline size_t __size_to_class(size_t x) {
  assert(x >= 0 && x < NUM_CLASSES);
  return class_sizes[x];
}

static inline const size_t class_for_size(size_t x) {
  size_t s;
  for (s = 0; s < NUM_CLASSES; s++) {
    if (class_sizes[s] >= x) {
      return s;
    }
  }
  abort();
}


#if __SIZEOF_POINTER__ == 8
// 64b pointers
#define combined_page_t unsigned __int128
#else
#define combined_page_t uint64_t
#endif

typedef struct {
  union {
    struct {
      volatile struct ipa_page_t * next_page;
      volatile int next_pg_name;
    };
    combined_page_t combined;
  };
} ipa_page_t;

typedef union {
#ifdef BOMALLOC_ALIGN_HEADERS
  //for header alignment to cache line bounds
  void * ____padding[64/sizeof(void*)];
#endif
  struct {
    void * payload; // This be set once and not made globally visable until set
    size_t size; // This be set once and not made globally visable until set
    volatile node_t seq_next;
    volatile node_t spec_next;
    pid_t allocator;
  };
} header_t;

/*
  Header pages contain a list of (possibly unallocated) headers
  Each header correspondes to one block (contig. memory for prog data).
  Additionally, the header page struct keeps track of the next unallocated
  header index within this page.

*/

#if __SIZEOF_POINTER__ == 8
// 64b pointers
#define combinded_header_next_t unsigned __int128
#else
#define combinded_header_next_t uint64_t
#endif

#ifndef PAGES_PER_HPG
#define PAGES_PER_HPG 5
#endif
#define HEADERS_PER_PAGE ((( PAGES_PER_HPG * PAGE_SIZE) - \
                          (sizeof(size_t) + sizeof(void*) + sizeof(ipa_page_t))) / sizeof(header_t) \
                          )

typedef struct {
  ipa_page_t next_page;
  volatile struct header_page_t * next_header;
  // unsigned number; // to be used used to help minimize the number of header pages?
  size_t next_free;
  header_t headers[HEADERS_PER_PAGE];
} header_page_t;

_Static_assert(sizeof(header_page_t) <= (PAGES_PER_HPG *PAGE_SIZE), "Header pages not configured correctly");

typedef struct {
  header_t * header;
} block_t;

typedef struct {
  ipa_page_t next_page;
  volatile struct huge_block_t * next_block;
  size_t huge_block_sz; //Note: this includes the block_t space
  int file_name; // Might be able to eliminate this field
  bool is_shared;
} huge_block_t;

_Static_assert(__builtin_offsetof(huge_block_t, next_page) == 0, "Bad struct: huge_block_t");
_Static_assert(__builtin_offsetof(header_page_t, next_page) == 0, "Bad struct: header_page_t");


// Used to collect statics with minimal cache impact
typedef volatile unsigned long stats_int_t __attribute__ ((aligned (64))); //64B aligned int

// Allocated at the start of the program in shared mem.
typedef struct {
  ipa_page_t next_page;
#ifdef COLLECT_STATS
  volatile stats_int_t allocations;
  volatile stats_int_t frees;
  volatile stats_int_t sbrks;
  volatile unsigned long allocs_per_class[NUM_CLASSES];
  volatile stats_int_t huge_allocations;
  volatile stats_int_t header_pages;
  volatile stats_int_t total_blocks; // this includes spec and non spec
  volatile stats_int_t total_alloc; // total space allocated from the system (heap + mmap)
  volatile stats_int_t time_malloc;
  volatile stats_int_t num_unmaps;
  volatile stats_int_t number_mmap; // how many pages where mmaped (headers & large)
  volatile stats_int_t spec_sbrks;
  volatile stats_int_t total_frames; //estimate of the total number of frames used
#endif
  volatile ipa_stack_t seq_free[NUM_CLASSES]; // sequential free list
  volatile ipa_stack_t spec_free[NUM_CLASSES]; // speculative free list
  volatile size_t base; //where segment begins (cache)
  volatile size_t spec_growth; // grow (B) done by spec group
  volatile void * spec_base;
  volatile unsigned next_name; // next header file to use
  volatile header_page_t * header_pg; // the address of the first header mmap page
  volatile huge_block_t * large_block; // pointer into the list of large blocks
  volatile int dummy; // used in unit tests
} shared_data_t;


static inline void stats_collect(volatile stats_int_t * x, unsigned increment) {
#ifdef COLLECT_STATS
  __sync_add_and_fetch(x, increment);
#endif
}

static inline void page_collect(volatile stats_int_t * x, unsigned s) {
  double frames = ((double) s) / PAGE_SIZE;
  stats_collect(x, ceil(frames));
}

// Extern data
extern shared_data_t * shared;

// Function prototypes

void ipa_init(void);

// Utility functions
static inline block_t * getblock(void * user_payload) {
  return (block_t *) (((char*) user_payload) - sizeof(block_t));
}

static inline huge_block_t * gethugeblock(void * user_payload) {
  return (huge_block_t *) (((char*) user_payload) - sizeof(huge_block_t));
}

static inline void * getpayload(volatile block_t * block) {
  return (void *) (((char*) block) + sizeof(block_t));
}

static inline void * gethugepayload(volatile huge_block_t * block) {
  return (void *) (((char*) block) + sizeof(huge_block_t));
}


#define SPEC_ALLOC_B (1<<0)
#define SEQ_ALLOC_B (1<<1)

static inline void * payload(volatile header_t * header) {
  return (void *) (((size_t) header->payload) & ~(SPEC_ALLOC_B | SEQ_ALLOC_B));
}

static inline void record_mode_alloc(volatile header_t * header) {
  if (speculating()) {
    __sync_fetch_and_or((int *) &header->payload, SPEC_ALLOC_B);
  } else {
    __sync_fetch_and_or((int *) &header->payload, SEQ_ALLOC_B);
  }
}

static inline bool seq_alloced(volatile header_t * header) {
  return (((size_t) header->payload) & SEQ_ALLOC_B) != 0;
}

static inline bool spec_alloced(volatile header_t * header) {
  return (((size_t) header->payload) & SPEC_ALLOC_B) != 0;
}

static inline void record_mode_free(volatile header_t * header) {
  if (speculating()) {
    __sync_fetch_and_and((int *) &header->payload, ~SPEC_ALLOC_B);
  } else {
    __sync_fetch_and_and((int *) &header->payload, ~SEQ_ALLOC_B);
  }
}

// Functions to convert between node links to the header
#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - __builtin_offsetof(type,member) );})

static inline volatile header_t * spec_node_to_header(volatile node_t * node) {
  if (node == NULL) {
    return NULL;
  } else {
    return container_of(node, volatile header_t, spec_next);
  }
}

static inline volatile header_t * seq_node_to_header(volatile node_t * node) {
  if (node == NULL) {
    return NULL;
  } else {
    return container_of(node, volatile header_t, seq_next);
  }
}

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

void * ipa_malloc(size_t);
void * ipacalloc(size_t, size_t);
void * iparealloc(void *, size_t);
void ipafree(void *);
size_t ipa_usable_space(void *);

void beginspec(void);
void endspec(bool);

void record_allocation(void *, size_t);

void ipa_teardown(void);
#endif
