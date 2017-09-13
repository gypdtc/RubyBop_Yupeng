#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

int main() {
  int * payload = calloc(sizeof(int), 1);
  printf("ipa payload %p\n", payload);
  *payload = 42;
  printf("Override allocation worked\n");
  free(payload);
  printf("Allocation freed\n");
  print_ipa_stats();
  return 0;
}
