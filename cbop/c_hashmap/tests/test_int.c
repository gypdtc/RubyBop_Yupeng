#include "stdio.h"
#include "assert.h"
#include "../hashmap.h"
#include "string.h"
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

int main(){
   srand(time(NULL));
   map_t hash = hashmap_new(sizeof(int));
   char s[100];
   int i;
   for(i = 0 ; i < 100 ; i++)
	strcat(s , "a");

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
   
   char key[100];
   int rand_index[10] = {1,82,93,4,55,66,37,28,49,12}; 
  
   for(i = 0 ; i < 100  ; i++){
   	memcpy(key , s , i + 1);
	key[i + 1] = '\0';
	assert(hashmap_put(hash , key ,&i) == MAP_OK);
   }

   for(i = 0 ; i < 10 ; i++){
	int l = rand_index[i];
	memcpy(key , s , l + 1);
	key[l + 1] = '\0';
   	assert(hashmap_remove(hash , key) == MAP_OK);
	assert(hashmap_remove(hash , key) == MAP_MISSING);
   }

   int j;
   for(i = 0 ; i < 100 ; i++){
	memcpy(key , s , i + 1);
	key[i + 1] = '\0';
	bool flag = true;
	for(j = 0 ; j < 10 ; j++){
		if (rand_index[j] == i){
			flag = false;
			break;
		}
	}
	void *result;
	if(!flag){
		assert(hashmap_get(hash , key , &result) == MAP_MISSING);
		continue;
	}
	assert(hashmap_get(hash , key , &result) == MAP_OK);
	int *value = (int *)result;
	assert((*value) == i);
   }
  
   hashmap_free(hash);
 
   printf("all tests passed.\n");
   
   return 0;
}
