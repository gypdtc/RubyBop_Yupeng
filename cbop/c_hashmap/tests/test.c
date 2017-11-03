#include "stdio.h"
#include "assert.h"
#include "../hashmap.h"

int main(){
   map_t hash = hashmap_new();
   int a = 3;
   assert(hashmap_put(hash , "test" ,&a) == MAP_OK);
   void *b;
   assert(hashmap_get(hash , "test" ,&b) == MAP_OK);
   int *p = (int *)b; 
   assert((*p) == 3);
   int c = 4;
   assert(hashmap_put(hash , "test" ,&c) == MAP_OK);
   assert(hashmap_get(hash , "test" ,&b) == MAP_OK);
   p = (int *) b;
   assert((*p) == 4); 
   assert(hashmap_remove(hash , "test") == MAP_OK);
   assert(hashmap_get(hash , "test" ,&b) == MAP_MISSING);

   printf("Tests passed\n"); 
   
   return 0;
}
