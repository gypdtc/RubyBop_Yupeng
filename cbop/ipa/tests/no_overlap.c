#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

#define NUM_ROUNDS 50

size_t class_for_rnd(int rnd) {
  return ALIGN(CLASS_TO_SIZE(rnd % NUM_CLASSES) - sizeof(block_t));
}

typedef struct {
  int * space;
  size_t size;
} record_t;

int main() {
  int rnd, check, block;
  size_t alloc_size;

  srand(0);

  record_t ptrs[NUM_ROUNDS] = {NULL};

  for (rnd = 0; rnd < NUM_ROUNDS; rnd++) {
    alloc_size = class_for_rnd(rnd);
    int * payload = ipa_malloc(alloc_size);
    ptrs[rnd].space = payload;
    ptrs[rnd].size = alloc_size;
    for (block = 0; block < alloc_size / sizeof(int); block++) {
      payload[block] = rnd;
    }
    for (check = 0; check < rnd; check++) {
      for(block = 0; block < ptrs[check].size / sizeof(int); block++) {
        assert(ptrs[check].space[block] == check);
      }
    }
  }
  printf("Basic allocation test passed\n");
  print_ipa_stats();
}
