#include "ipa.h"

#ifdef IPA_SYSTEM_ALLOC
#if defined(__GNUC__) && !defined(NO_HOOK)
#include <malloc.h>

static void * ipa_hook (size_t size, const void * c){
  return ipa_malloc(size);
}

static void ipafree_hook (void* payload, const void * c){
  ipafree(payload);
}

static void * iparealloc_hook(void * payload, size_t size, const void * c) {
  return iparealloc(payload, size);
}

static void * ipacalloc_hook(size_t a, size_t b, const void * c) {
  return ipacalloc(a, b);
}

static void ipa_dehook() {
  __malloc_hook = NULL;
  __free_hook = NULL;
  __realloc_hook = NULL;
#ifdef __calloc_hook
  __calloc_hook = NULL;
#endif

}

void __attribute__((constructor)) ipa_hook() {
  atexit(ipa_dehook);
  __malloc_hook = ipa_hook;
  __free_hook = ipafree_hook;
  __realloc_hook = iparealloc_hook;
#ifdef __calloc_hook
  __calloc_hook = ipacalloc_hook;
#endif
}

#else

void * malloc(size_t t) {
  return ipa_malloc(t);
}
void free(void * p) {
  ipafree(p);
}
size_t malloc_usable_size(void * p) {
  return ipa_usable_space(p);
}
void * realloc(void * p, size_t size) {
  return iparealloc(p, size);
}
void * calloc(size_t a, size_t b) {
  return ipacalloc(a, b);
}
#endif
#endif
