#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "ipa.h"
#include "ipa_utils.h"

int num_rounds = 200;
int num_children = 4;
size_t alloc_size = sizeof(int);

#define PER_EACH (num_rounds / num_children)

// Random number generation based off of http://www.azillionmonkeys.com/qed/random.html
#define RS_SCALE (1.0 / (1.0 + RAND_MAX))


volatile bool spec = false;

void record_allocation(void * p, size_t s) {

}

bool speculating() {
  return spec;
}

int getuniqueid() {
  return (int) getpgid(getpid());
}

void __attribute__((noreturn)) child(const int id)  {
  spec = true;
  while (shared->dummy == 0) {
    ;
  }
  assert(spec);
  // printf("child %d (pid %d) sbrk at start %p\n", id, getpid(), start_ds);
  for (size_t  rnd = PER_EACH * id; rnd < PER_EACH * (id + 1); rnd++) {
    int * payload = ipa_malloc(alloc_size);
    *payload = 0xdeadbeef;
    printf("child %d Allocated %p\n", id, payload);
  }
  printf("Child %d exits\n", id);
  exit(0);
}

pid_t parent;

int main(int argc, char ** argv) {
  for (int i = 0; i < argc - 1; i++) {
    if (!strcmp(argv[i], "-i")) {
      num_rounds = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-t")) {
      num_children = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-s")) {
      alloc_size = atoi(argv[++i]);
      if (alloc_size == -1) {
        alloc_size = MAX_SIZE + sizeof(block_t) + 1;
      }
    }
  }
  printf("Running with %u children %u total allocations of size %lu\n",
        num_children, num_rounds, alloc_size);
  srand(0);
  pid_t children[num_children];
  parent = getpid();
  spec = true;
  beginspec();
  printf("sbrk start = %p\n", sbrk(0));
  for (int i = 0; i < num_children; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      child(i);
    } else if (pid == -1) {
      perror("Unable to fork process");
      exit(-1);
    }
    children[i] = pid;
  }
  shared->dummy = 1;
  int status;
  for (int i = 0; i < num_children; i++) {
    if (waitpid(children[i], &status, 0) == -1) {
      perror("Unable to wait for child");
      exit(-1);
    }
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
  }
  endspec(true);
  spec = false;
  printf("Spec allocation test finished!\n");
  print_ipa_stats();
  ipa_teardown();
}
