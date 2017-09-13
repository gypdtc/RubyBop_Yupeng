#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

#define NUM_ROUNDS 2500

// Random number generation based off of http://www.azillionmonkeys.com/qed/random.html
#define RS_SCALE (1.0 / (1.0 + RAND_MAX))

size_t random_class () {
    double d;
    do {
       d = (((rand () * RS_SCALE) + rand ()) * RS_SCALE + rand ()) * RS_SCALE;
    } while (d >= 1); /* Round off */
    return d * (NUM_CLASSES + 1);
}

size_t uniform_size_class() {
  size_t klass = random_class();
  if (klass >= NUM_CLASSES) {
    return MAX_SIZE + sizeof(block_t) + 1;
  }
  return ALIGN(CLASS_TO_SIZE(klass) - sizeof(block_t));
}

int main() {
  int rnd, check;
  size_t alloc_size;

  srand(0);

  int * ptrs[NUM_ROUNDS] = {NULL};

  for (rnd = 0; rnd < NUM_ROUNDS; rnd++) {
    alloc_size = uniform_size_class();
    int * payload = ipa_malloc(alloc_size);
    size_t usable = ipa_usable_space(payload);
    assert(payload != NULL);
    assert(usable >= alloc_size);
    ptrs[rnd] = payload;
    for (check = 0; check < rnd; check++) {
      if (ptrs[check] == payload) {
        fprintf(stderr, "Duplicate allocation found");
        exit(-1);
      }
    }
    *payload = 42;
  }
  printf("Basic allocation test passed\n");
  print_ipa_stats();
}
