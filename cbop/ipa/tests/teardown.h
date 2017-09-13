#ifndef __TEST_TEARDOWN
#define __TEST_TEARDOWN
#include "ipa.h"

static void __attribute__((destructor)) test_teardown() {
  ipa_teardown();
}

#endif
