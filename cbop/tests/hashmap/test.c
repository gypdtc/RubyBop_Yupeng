#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "unistd.h"
#include "../../../c_hashmap/hashmap.h"
#include "../../bop_api.h"
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
   
   char key[50] = "";
   int i;
   for(i = 0 ; i < 10 ; i++){
       BOP_ppr_begin(1); 
           strcat(key , "a");
           assert(hashmap_put(hash , key ,&i) == MAP_OK);
       	   sleep(2);
	   
	   
       BOP_ppr_end(1);
   }

   key[0] = '\0';
   int sum = 0;
   for(i = 0 ; i < 10 ; i++){
  	void *result;
	strcat(key , "a");
	assert(hashmap_get(hash , key  ,&result) == MAP_OK);
	int *p = (int *)result;
	sum += *p;
   }

   assert(sum == 45);
   printf("Tests passed\n"); 
   
   return 0;
}
