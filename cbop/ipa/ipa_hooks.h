#ifndef __BOMALLOC_HOOKS
#define __BOMALLOC_HOOKS
#include <stdlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

int getuniqueid(void);
bool speculating();
void record_allocation(void *, size_t);

#endif
