#ifndef ___BOMALLOC_STACK_H
#define ___BOMALLOC_STACK_H
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#if __SIZEOF_POINTER__ == 8
// 64b pointers
#define combined_stack_t unsigned __int128
#define stack_age_t uint64_t
#else
#define combined_stack_t uint64_t
#define stack_age_t uint32_t
#endif

typedef struct {
  volatile struct node_t * next;
} node_t;

typedef union {
  struct {
    volatile node_t * head;
    volatile stack_age_t age;
  };
  combined_stack_t combined;
}
#ifdef BOMALLOC_ALIGN_STACKS
__attribute__((aligned (64)))
#endif
ipa_stack_t;


static inline bool empty(volatile ipa_stack_t * stack) {
  return stack->head == NULL;
}

static inline void init_stack(ipa_stack_t * stack) {
  stack->head = NULL;
  stack->age = 0;
}

// The function below can be used as a more light-weight 'semi-atomic' load without spinning
//Loading the variable used to prevent the ABA problem first is suffient -- read barrier to prevent proc. reordering
static inline ipa_stack_t naba_load(volatile ipa_stack_t * stack)  {
  ipa_stack_t load;
#if defined(__x86_64__) || defined(__i386__)
  // x86 has a strong enough memory model that a runtime memory fence is not needed.
  // A compiler fence is needed to keep the compiler from reordering
  // https://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/
  asm volatile("": : :"memory");
  load.age = stack->age;
  asm volatile("": : :"memory");
  load.head = stack->head;
#else
  // Need a barrier -- really only read but no good GCC binding
  load.age = stack->age;
  __sync_synchronize();
  load.head = stack->head;
#endif
  return load;
}

static inline void push(volatile ipa_stack_t * stack, volatile node_t * item) {
  ipa_stack_t new_stack, expected;
  do {
    expected = naba_load(stack);
    new_stack.age = expected.age + 1;
    new_stack.head = item;
    item->next = (struct node_t *) expected.head;
  } while(!__sync_bool_compare_and_swap(&stack->combined,
                                         expected.combined,
                                         new_stack.combined));
}

static inline volatile node_t * pop(volatile ipa_stack_t * stack) {
  while (true) {
    ipa_stack_t expected = naba_load(stack);
    if (expected.head == NULL) {
      return NULL;
    } else {
      ipa_stack_t new_stack;
      new_stack.age = expected.age + 1;
      __sync_synchronize(); //More fine-grain than the naba load -- let the 1 happen 'whenever'
      new_stack.head = (node_t *) expected.head->next;
      if (__sync_bool_compare_and_swap(&stack->combined,
                                        expected.combined,
                                        new_stack.combined)) {
        return expected.head;
      }
    }
  }
}

static inline void push_ageless(ipa_stack_t * stack, node_t * node) {
  node->next = (struct node_t *) stack->head;
  stack->head = node;
}

static inline volatile node_t * pop_ageless(volatile ipa_stack_t * stack) {
  if (empty(stack)) {
    return NULL;
  } else {
    volatile node_t * pop = stack->head;
    stack->head = (node_t *) pop->next;
    stack->age++;
    return pop;
  }
}

static inline ipa_stack_t * new_stack() {
  return calloc(1, sizeof(ipa_stack_t));
}

#endif
