#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

int main() {
  int * payload = ipa_malloc(sizeof(int));
  printf("ipa payload %p\n", payload);
  *payload = 42;
  printf("Basic allocation test passed\n");
  ipafree(payload);
  print_ipa_stats();
  return 0;
}
