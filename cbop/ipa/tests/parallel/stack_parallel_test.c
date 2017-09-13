#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "stack.h"
#include "timing.h"

typedef struct  {
  node_t node;
  int data;
} data_node_t;

data_node_t * alloc_node(int data) {
  data_node_t * node = calloc(1, sizeof(data_node_t));
  node->data = data;
  return node;
}

#define elements 200000
#define num_threads 4
#define per_thread (elements / num_threads)

bool elements_found[elements] = {false};
bool errored = false;
ipa_stack_t stack;

void * worker_thread(void * id_ptr) {
  int id = * (int *) id_ptr;
  free(id_ptr);
  for (size_t i = 0; i < per_thread; i++) {
    push(&stack, (node_t *) alloc_node(id * per_thread + i));
  }
  for (int i = 0; i < per_thread; i++) {
    data_node_t * data = (data_node_t *) pop(&stack);
    if (elements_found[data->data]) {
      fprintf(stderr, "Found duplicate %d\n", data->data);
      errored = true;
    }
    elements_found[data->data] = true;
  }
  return NULL;
}

int main() {
  pthread_t threads[num_threads] = {0};

  for(int i = 0; i < num_threads; i++) {
    int * data = malloc(sizeof(int));
    *data = i;
    if(pthread_create(&threads[i], NULL, worker_thread, data)) {
      perror("Unable to create thread");
      exit(-1);
    }
  }

  for(int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL)) {
      perror("Unable to join thread");
      exit(-1);
    }
  }

  for (int i = 0; i < elements; i++) {
    if (!elements_found[i]) {
      errored = true;
      fprintf(stderr, "Never found %d\n", i);
    }
  }
  if (errored) {
    printf("Parallel stack test failed\n");
  } else {
    printf("Parallel stack test passed\n");
  }
  exit(errored ? EXIT_FAILURE : EXIT_SUCCESS);
}
