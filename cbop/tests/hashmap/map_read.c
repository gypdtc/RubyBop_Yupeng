#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "unistd.h"
#include "../../c_hashmap/hashmap.h"
#include "../../bop_api.h"

#define STRSIZE 50

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
   
   char keyall[50] = "";
   int i;
   for(i = 0 ; i < 10 ; i++){ 
   	char key[STRSIZE];
    	memcpy(key, keyall, i + 1);
     	key[i + 1] = '\0';
      	assert(hashmap_put(hash, key, &i) == MAP_OK);
   }
   
   printf("Puting all 10 elements in the hashmap\n");

   int sum = 0;
   for(i = 0 ; i < 10 ; i++){
  	void *result;
	BOP_ppr_begin(1);
		char key[STRSIZE];
        	memcpy(key, keyall, i + 1);
        	key[i + 1] = '\0';
		assert(hashmap_get(hash , key  ,&result) == MAP_OK);
		int *p = (int *)result;
		sum += *p;
		BOP_record_read(&sum , sizeof(int));
		BOP_record_write(&sum , sizeof(int));
		sleep(1);
	BOP_ppr_end(1);
   }
   
   printf("reading all elements from hashmap,the final sum is %d\n",sum);
   assert(sum == 45);
   printf("Tests passed\n"); 
   
   return 0;
}