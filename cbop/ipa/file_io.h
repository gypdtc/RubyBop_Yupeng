#ifndef ___BOMALLOC_FILE_IO
#define ___BOMALLOC_FILE_IO
#include <stdlib.h>
#include <stdbool.h>

int mmap_fd(unsigned, size_t);
size_t get_size_fd(int);
size_t get_size_name(unsigned);



#endif
