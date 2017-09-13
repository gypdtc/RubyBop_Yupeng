#include <stdlib.h>
#include <stdio.h>
#include "ipa.h"
#include "dummy.h"
#include "teardown.h"

int main() {
  int * payload = ipa_malloc(sizeof(int));
  printf("ipa payload %p\n", payload);
  *payload = 42;
  ipafree(payload);
  printf("Basic free test passed\n");
  return 0;
}
