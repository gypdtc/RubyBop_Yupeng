#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <error.h>
#include <ftw.h>
#include <unistd.h>
#include <signal.h>

#include "ipa.h"
#include "memmap.h"
#include "stack.h"
#include "ipa_utils.h"
#include "timing.h"


shared_data_t * shared;
volatile header_page_t * first_full = NULL;
volatile size_t my_growth;

extern header_page_t * seq_headers;

static ipa_stack_t delayed_frees_unclaimable[NUM_CLASSES] = {0};
static ipa_stack_t delayed_frees_reclaimable[NUM_CLASSES] = {0};

//prototypes
void * inc_heap(intptr_t);
void free_delayed(void);

void * end_ds = NULL;

bool out_of_range(void * payload) {
  return payload < (void*) shared->base || payload >= end_ds;
}

#ifdef SHARED_SPEC_BLOCKS
static bool fault_on_pop = false;
void map_all_segv(int signo) {
  fault_on_pop = true;
  map_missing_pages_handler();
}
#endif

static inline volatile header_t * alloc_pop(size_t size) {
  size_t index = SIZE_TO_CLASS(size);
  assert(index >= 0 && index < NUM_CLASSES);
  if (speculating()) {
    if (!empty(&delayed_frees_reclaimable[index])) {
      return spec_node_to_header(pop_ageless(&delayed_frees_reclaimable[index]));
    } else {
#ifdef SHARED_SPEC_BLOCKS
      typedef void (*sighandler_t)(int);
      volatile header_t * h = NULL;
      sighandler_t old = signal(SIGSEGV, map_all_segv);
      do {
        fault_on_pop = false;
        h = spec_node_to_header(pop(&shared->spec_free[index]));
      } while(h == NULL && fault_on_pop);
      signal(SIGSEGV, old);
      return h;
#else
      return spec_node_to_header(pop(&shared->spec_free[index]));
#endif
    }
  } else {
    // Because pages are never backed to disk and the shared
    // lists are now only used by the speculative group,
    // the monitor process and a seq. program will see different
    // free lists / header pages meaning we do not need to synchronize
#ifdef SUPPORT_THREADS
    return seq_node_to_header(pop(&shared->seq_free[index]));
#else
   return seq_node_to_header(pop_ageless(&shared->seq_free[index]));
#endif
  }
}

size_t ipa_usable_space(void * payload) {
  if (payload == NULL) {
    return 0;
  } else if (out_of_range(payload)) {
    return gethugeblock(payload)->huge_block_sz - sizeof(huge_block_t);
  } else {
    return getblock(payload)->header->size - sizeof(block_t);
  }
}

static void map_headers(char * begin, size_t index, size_t num_blocks) {
  size_t i, header_index = -1;
  volatile header_page_t * page;
  size_t block_size = CLASS_TO_SIZE(index);
  volatile ipa_stack_t * seq_stack = &shared->seq_free[index];
  // volatile ipa_stack_t * spec_stack = &shared->spec_free[index];
  ipa_stack_t * local_stack = &delayed_frees_reclaimable[index];
  volatile block_t * block;
  assert(block_size == ALIGN(block_size));
  volatile header_t * header = NULL;
  const bool am_spec = speculating();

  for (i = 0; i < num_blocks; i++) {
    volatile header_page_t * start_page = am_spec ? shared->header_pg : seq_headers;
    while (header_index >= (HEADERS_PER_PAGE - 1) || header_index == -1) {
      for (page = start_page; page != NULL && page != first_full; page = (volatile header_page_t *) page->next_header) {
        while (am_spec && !is_mapped((void *) page)) {
          map_missing_pages();
        }
        if (page->next_free <= HEADERS_PER_PAGE - 1) {
          header_index = __sync_fetch_and_add(&page->next_free, 1);
          if (header_index < HEADERS_PER_PAGE) {
            goto found;
          }
        }
      }
      if (page == NULL || page == first_full) {
        page = allocate_header_page();
        header_index = 0;
        first_full = start_page;
        goto found;
      }
    }
    found: block = (block_t *) (&begin[i * block_size]);

    header = &page->headers[header_index];

    header->size = block_size;
    // mmaped pages are padded with zeros, set NULL anyways
    header->spec_next.next = NULL;
    header->seq_next.next = NULL;
    header->payload = getpayload(block);

    block->header = (header_t * ) header;
    // (in)sanity checks
    assert(getblock(getpayload(block)) == block);
    assert(getblock(getpayload(block))->header == header);
    assert(payload(header) == getpayload(block));

    if (am_spec) {
      push_ageless(local_stack, (node_t *) &header->spec_next);
      // push_ageless(local_stack, (node_t *) &header->spec_next);
    } else {
      __sync_synchronize(); // mem fence
      push(seq_stack, (node_t * ) &header->seq_next);
    }
    // get the i-block
    header_index = -1;
  }
}

void ipa_init() {
  if (shared == NULL) {
    shared = mmap(NULL,
              MAX(sizeof(shared_data_t), PAGE_SIZE),
              PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    bzero(shared, PAGE_SIZE);
    shared->number_mmap = ceil(((double) sizeof(shared_data_t)) / PAGE_SIZE);
    shared->base = (size_t) sbrk(0); // get the starting address
    page_collect(&shared->total_frames, sizeof(shared_data_t));
#ifdef COLLECT_STATS
    shared->total_alloc = MAX(sizeof(shared_data_t), PAGE_SIZE);
#endif
    shared->next_page.next_pg_name = 0;
    shared->next_page.next_page = NULL;
  }
}
static void grow(size_t aligned) {
  // Need to grow the space
  size_t index = class_for_size(aligned);
  size_t size = CLASS_TO_SIZE(index);
  assert(size > 0);
  size_t blocks = MIN(HEADERS_PER_PAGE, MAX(PAGE_SIZE / size, 15));
  char * base;

  const intptr_t my_region_size = size * blocks;
  if (speculating()) {
    // first allocate the extra space that's needed. Don't record the to allocation
    const intptr_t total = __sync_add_and_fetch(&shared->spec_growth, my_region_size);
    const intptr_t sync_space = total - my_region_size;
    // Below, we grow to match the agreed global speculative heap
    // This is done to re-use code for the normal nonspec path
    // which will grow the heap by the amount we need, eg. my_region_size
    assert(total - my_growth - my_region_size >= 0);
    assert(total >= my_region_size);
    // printf("pid %d total %d my_growth %lu\n", getpid(), total, my_growth);
    inc_heap(total - my_growth);
    base = ((char *) shared->spec_base) + sync_space;
    stats_collect(&shared->spec_sbrks, 1);
    my_growth += my_region_size;
  } else {
     base = inc_heap(my_region_size);
  }
  // Grow the heap by the space needed for my allocations
  // char * base = inc_heap(my_region_size);
  if (speculating()) {
  }
  // now map headers for my new (private) address region
  map_headers(base, index, blocks);
  stats_collect(&shared->total_blocks, blocks);
  page_collect(&shared->total_frames, my_region_size);
}

void * inc_heap(intptr_t s) {
  stats_collect(&shared->sbrks, 1);
  stats_collect(&shared->total_alloc, s);
  void * x = sbrk(s);
  if (x == (void *) -1) {
    ipa_perror("Unable to extend data segment");
    abort();
  }
  end_ds = sbrk(0);
  return x;
}

size_t stack_for_size(size_t min_size) {
  size_t klass;
  for (klass = 0; CLASS_TO_SIZE(klass) < min_size && klass < NUM_CLASSES; klass++) {
    ;
  }
  if (klass >= NUM_CLASSES) {
    return -1;
  }
  return CLASS_TO_SIZE(klass);
}

void * ipa_malloc(size_t size) {
  volatile header_t * header;
  size_t aligned = ALIGN(size + sizeof(block_t));
  if (shared == NULL) {
    ipa_init();
  }
#ifdef COLLECT_STATS
  stats_collect(&shared->allocations, 1);
  struct timespec start = timer_start();
#endif
  if (size == 0) {
    return NULL;
  } else if (size > MAX_SIZE) {
    huge_block_t * block = allocate_large(aligned);
    if (block != NULL) {
      record_allocation(block, block->huge_block_sz);
    } else {
      ipa_perror("Unable to allocate large user payload");
      return NULL;
    }
    return gethugepayload(block);
  } else {
    for (header = alloc_pop(aligned); header == NULL; header = alloc_pop(aligned)) {
      grow(aligned);
    }
    assert(payload(header) != NULL);
    // Ensure that the payload is in my allocated memory
#ifdef SHARED_SPEC_BLOCKS
    if (out_of_range(payload(header))) {
      // heap needs to extend to header->payload + header->size
      void * pl = payload(header);
      block_t * block = getblock(pl);
      void * end_block = (void *) ((intptr_t) block) + header->size;
      intptr_t growth = ((intptr_t) end_block) - ((intptr_t) sbrk(0));
      my_growth += growth;
      inc_heap(growth);
      block->header = (header_t *) header;
      // getblock(payload(header))->header = (header_t *) header;
      abort();
    }
#endif
    if (getblock(payload(header))->header != header) {
      getblock(payload(header))->header = (header_t *) header;
    }
    assert(payload(header) != NULL);
    assert(getblock(payload(header))->header == header);
#ifdef COLLECT_STATS
      stats_collect(&shared->time_malloc, timer_end(start));
      stats_collect(&shared->allocs_per_class[class_for_size(aligned)], 1);
#endif
    record_mode_alloc(header);
    record_allocation(payload(header), header->size);
    if (speculating()) {
      header->allocator = getpid();
    }
    assert(ALIGN((size_t) payload(header)) == (size_t) payload(header));
    return payload(header);
  }
}

void ipafree(void * payload) {
  stats_collect(&shared->frees, 1);
  if (payload == NULL) {
    return;
  }

  volatile header_t * header = getblock(payload)->header;

  if (out_of_range(payload)) {
    // A huge block is unmapped directly to kernel
    // This can be done immediately -- there is no re-allocation conflicts
    if (!speculating()) {
      huge_block_t * block = gethugeblock(payload);
      if (munmap(block, block->huge_block_sz) == -1) {
        ipa_perror("Unable to unmap block");
      }
    }
  } else if (!speculating()) {
    // Not speculating -- free now
    volatile ipa_stack_t * stack = &shared->seq_free[SIZE_TO_CLASS(header->size)];
    record_mode_free(header);
    push(stack, &header->seq_next);
  } else if (header->allocator == getpid()) {
    // This is reclaimable
    size_t index = SIZE_TO_CLASS(ipa_usable_space(payload));
    push_ageless(&delayed_frees_reclaimable[index], (node_t *) &header->spec_next);
  } else {
    /**
     * We can use the speculative next. If the payload was allocated speculatively,
     * 	then the spec next is not needed
     * If it was originally allocated sequentially (eg. before starting spec)
     * 	then spec_next was unused -- no need to keep around
     */
    size_t index = SIZE_TO_CLASS(ipa_usable_space(payload));
    push_ageless(&delayed_frees_unclaimable[index], (node_t*) &getblock(payload)->header->spec_next);
  }
}

void free_delayed() {
  size_t index;
  for (index = 0; index < NUM_CLASSES; index++) {
    while (!empty(&delayed_frees_unclaimable[index])) {
      volatile node_t * node = pop_ageless(&delayed_frees_unclaimable[index]);
      volatile header_t * head = container_of(node, volatile header_t, spec_next);
      push_ageless((ipa_stack_t *) &shared->seq_free[index], (node_t *)  &head->seq_next);
    }
    while (!empty(&delayed_frees_reclaimable[index])) {
      volatile node_t * node = pop_ageless(&delayed_frees_reclaimable[index]);
      volatile header_t * head = container_of(node, volatile header_t, spec_next);
      push_ageless((ipa_stack_t *) &shared->seq_free[index], (node_t *) &head->seq_next);
    }
  }
}

void * ipacalloc(size_t nmemb, size_t size) {
  void * payload = ipa_malloc(nmemb * size);
  if (payload != NULL) {
    memset(payload, 0, ipa_usable_space(payload));
  }
  return payload;
}


void * iparealloc(void * p, size_t size) {
  size_t original_size = ipa_usable_space(p);
  if (original_size >= size) {
    return p;
  } else {
    void * new_payload = ipa_malloc(size);
    memcpy(new_payload, p, original_size);
    ipafree(p);
    return new_payload;
  }
}
