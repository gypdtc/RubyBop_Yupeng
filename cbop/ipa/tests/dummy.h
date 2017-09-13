#ifndef __BOMALLOC_TEST_DUMMY
#define __BOMALLOC_TEST_DUMMY
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipa_utils.h"


bool speculating() {
  return false;
}

void record_allocation(void * p, size_t t) {

}

int getuniqueid() {
  return (int) getpgid(getpid());
}

#endif
