#include <stdlib.h>
#include <stdio.h>
#include "stack.h"

typedef struct  {
  node_t node;
  int data;
} data_node_t;

data_node_t * alloc_node(int data) {
  data_node_t * node = calloc(1, sizeof(data_node_t));
  node->data = data;
  return node;
}


int main() {
  const int elements = 5;
  int index, errors = 0;
  volatile node_t * empty_stack;
  ipa_stack_t * stack = new_stack();

  for (index = 0; index < elements; index++) {
    push(stack, (node_t *) alloc_node(index));
  }
  for (index = elements - 1; index >= 0; index--) {
    data_node_t * popped = (data_node_t *) pop(stack);
    if (popped == NULL) {
      errors++;
      fprintf(stderr, "Stack returned null for index %d\n", index);
    } else if (popped->data != index) {
      errors++;
      fprintf(stderr, "Invalid item found in stack. Expected %d got %d", index, popped->data);
    }
  }
  if ((empty_stack = pop(stack)) != NULL) {
    errors++;
    fprintf(stderr, "Non-null return from empty stack (%p)\n", empty_stack);
  }
  if (errors == 0) {
    printf("Stack test past successfully\n");
  }
  exit(errors == 0 ? 0 : -1);
}
