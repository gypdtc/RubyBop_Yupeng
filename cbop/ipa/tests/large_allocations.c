#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

#define NUM_ROUNDS 5

size_t class_for_rnd(int rnd) {
  return ALIGN(CLASS_TO_SIZE(rnd % NUM_CLASSES) - sizeof(block_t));
}


int main() {
  int rnd, check;
  size_t alloc_size;

  srand(0);

  int * ptrs[NUM_ROUNDS] = {NULL};

  for (rnd = 0; rnd < NUM_ROUNDS; rnd++) {
    alloc_size = MAX_SIZE + sizeof(block_t) + 1;
    int * payload = ipa_malloc(alloc_size);
    ptrs[rnd] = payload;
    for (check = 0; check < rnd; check++) {
      assert(ptrs[check] != payload);
    }
  }
  printf("Basic allocation test passed\n");
  print_ipa_stats();
}
