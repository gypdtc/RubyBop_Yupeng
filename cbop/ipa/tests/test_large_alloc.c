#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

int main() {
  int * payload = ipa_malloc(MAX_SIZE + 1);
  printf("ipa payload %p\n", payload);
  *payload = 42;
  printf("Large allocation passed\n");
  ipafree(payload);
  printf("Successfully freed large block\n");
  print_ipa_stats();
  return 0;
}
