#ifndef _H_BOMALLOC_SYNC_H
#define _H_BOMALLOC_SYNC_H
#include <stdbool.h>

void beginspec();
void endspec(bool);
void synch_lists();
void promote_list();
void free_delayed(void);

static inline void set_large_perm();

extern void * inc_heap(size_t);
extern void ipa_init(void);


#endif
