#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "ipa.h"
#include "dummy.h"
#include "teardown.h"


void parent(int * payload1, pid_t child, void* value) {
  while (getblock(payload1)->header->spec_next.next != value) {
    ; // do nothing
  }
  assert(getblock(payload1)->header->spec_next.next == value);
  // kill(child, SIGINT);

}
extern shared_data_t * shared;

int main() {
  int * payload1 = ipa_malloc(sizeof(int));
  void * value = main;
  pid_t child = fork();
  if (child == 0) {
    // in the child process -- update something and then quit
    block_t * block = getblock(payload1);
    block->header->spec_next.next = value; // some field in the header -- shared memory
    // while(1) {
    //   ; // do nothing
    // }
    while (shared->dummy != -1) {
        ;
    }
    printf("Child exits\n");
  } else {
    while (getblock(payload1)->header->spec_next.next != value) {
      ; // do nothing
    }
    assert(getblock(payload1)->header->spec_next.next == value);
    shared->dummy = -1;
    printf("Header update test succeded\n");
  }
}
