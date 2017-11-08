#include "stdio.h"
#include "assert.h"
#include "../hashmap.h"
#include "string.h"
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

int main(){
 
   map_t hash = hashmap_new(-1);
 
   char s[100];
   char t[100];
   int i;
   for(i = 0 ; i < 100 ; i++)
   {
	strcat(t , "b");
	strcat(s , "a");
   }
   
   char key[100]; 
   char value[100];
   int rand_index[10] = {1,82,93,4,55,66,37,28,49,12}; 
  
   for(i = 0 ; i < 100  ; i++){
   	memcpy(key , s , i + 1);
	key[i + 1] = '\0';
	memcpy(value , t , i + 1);
	value[i + 1] = '\0';
	assert(hashmap_put(hash , key , value) == MAP_OK);
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
	memcpy(value , t  , i + 1);
	value[i + 1] = '\0';
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
	char *val = (char *)result;
	assert(strcmp(val , value) == 0);
   }
  
   hashmap_free(hash);
 
   printf("all tests passed.\n");
   
   return 0;
}
