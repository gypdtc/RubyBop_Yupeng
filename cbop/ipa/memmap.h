#ifndef ___HAVE_BOMALLOC_MMAP_H
#define ___HAVE_BOMALLOC_MMAP_H
#include <stdbool.h>
#include <stdlib.h>

#include "ipa.h"

header_page_t * allocate_header_page(void);
huge_block_t * allocate_large(size_t);
int mmap_fd(unsigned, size_t);
int mmap_existing_fd(unsigned);
volatile ipa_page_t * map_missing_pages(void);
void map_missing_pages_handler(void);
bool is_mapped(void *);
#endif
