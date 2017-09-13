#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "stack.h"
#include "ipa.h"
#include "ipa_sync.h"
#include "ipa_utils.h"
#include "memmap.h"

extern size_t my_growth;
extern shared_data_t * shared;
extern header_page_t * seq_headers;
extern header_page_t * seq_headers_last;
extern volatile header_page_t * first_full;

void beginspec() {
  assert(speculating()); // TODO -- probably special case this function
  // void * end_ds = sbrk(0);
  // printf("sbrk pre-spec 0x%p\n", end_ds);
  my_growth = 0;
  ipa_init();
  shared->spec_base = sbrk(0);
  map_missing_pages();
  synch_lists();
  set_large_perm(MAP_PRIVATE);
  first_full = NULL;
}

void endspec(bool ppr_won) {
  assert(speculating()); // TODO -- probably special case this function
  if (my_growth < shared->spec_growth) {
    inc_heap(shared->spec_growth - my_growth);
  }
  map_missing_pages();
  free_delayed();
  if (ppr_won) {
    promote_list();
    // Fix up the header lists
    if (seq_headers_last != NULL) {
      seq_headers_last->next_header = (volatile struct header_page_t *) shared->header_pg;
      shared->header_pg = NULL;
    }
    first_full = NULL;
  }
  // Note: pages are auto unmapped by the kernel on failure
}

static inline void set_large_perm(int flags) {
  volatile huge_block_t * block;
  huge_block_t data = {0};
  for (block = (volatile huge_block_t *) shared->large_block; block != NULL; block = (volatile huge_block_t *)  block->next_block) {
    if (block->is_shared) {
      int fd = mmap_existing_fd(block->file_name);
      data = *block;
      fsync(fd);
      if (!mmap((void *) block, data.huge_block_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) {
        ipa_perror("Unable to reconfigure permissions.");
      }
      close(fd);
      block->is_shared = false;
    }
  }
}

void synch_lists() {
  size_t i;
  volatile header_page_t * page;
  volatile header_t * header;
  // Set the heads of the stacks to the corresponding elements
  for (i = 0; i < NUM_CLASSES; i++) {
    if(shared->seq_free[i].head != NULL) {
      shared->spec_free[i].head = (node_t *) &seq_node_to_header(shared->seq_free[i].head)->spec_next;
    } else {
      shared->spec_free[i].head = NULL;
    }
  }

  // Set the stack elements
  for (page = seq_headers; page != NULL; page = (header_page_t *) page->next_header) {
    for (i = 0; i < MIN(HEADERS_PER_PAGE, page->next_free); i++) {
      header = &page->headers[i];
      if (!seq_alloced(header) && header->seq_next.next != NULL) {
        volatile header_t * next_header = seq_node_to_header((node_t*) header->seq_next.next);
        header->spec_next.next = (volatile struct node_t *) &next_header->spec_next;
      } else {
        header->spec_next.next = NULL;
      }
    }
  }
}

// This step can (may?) be eliminated by sticking the two items in an array and swaping the index
// mapping to spec / seq free lists
// pre-spec is still needed OR 3x wide CAS operations -- 2 for the pointers
// (which need to be adjacent to each other) and another for the counter
void promote_list() {
  size_t i;
  volatile header_page_t * page;
  volatile header_t * header;
  map_missing_pages();
  // Set the heads of the stacks to the corresponding elements
  for (i = 0; i < NUM_CLASSES; i++) {
    if(shared->spec_free[i].head != NULL) {
      shared->seq_free[i].head = (node_t *) &spec_node_to_header(shared->spec_free[i].head)->seq_next;
    } else {
      shared->seq_free[i].head = NULL;
    }
  }
  // Set the stack elements
  for (page = shared->header_pg; page != NULL; page = (header_page_t *) page->next_header) {
    for (i = 0; i < MIN(HEADERS_PER_PAGE, page->next_free); i++) {
      header = &page->headers[i];
      assert(payload(header) != NULL);
      if (!spec_alloced(header) && header->seq_next.next != NULL) {
        volatile header_t * next_header = spec_node_to_header((node_t *) header->spec_next.next);
        header->seq_next.next = (volatile struct node_t *) &next_header->seq_next;
      } else {
        header->seq_next.next = NULL;
      }
      header->payload = (void *) (((intptr_t) header->payload) & ~(SEQ_ALLOC_B | SPEC_ALLOC_B));
      header->allocator = 0;
    }
  }
}
