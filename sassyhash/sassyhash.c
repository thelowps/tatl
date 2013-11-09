/*
 * sassyhash.c
 *
 *
 */

#include "sassyhash.h"
#include "linked.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SASSY_HASH_DEFAULT_SIZE 100
const int PRIMES [26] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101};

//////////////////////////
// HASH TABLE UTILITIES //
//////////////////////////

//
// sh_calculate_hash
//
// Calculates the hash for the given key.
// RETURNS: the calculated hash
int sh_calculate_hash (struct shash* map, const char* key) {
  long long sig = 1;
  int i = 0;
  for (; i < strlen(key); ++i) {
    sig *= PRIMES[(*key)%26];
  }
  return (sig%map->_SIZE);
}


//
// sh_create_map
//
// Creates a new hash map. 'size' indicates the array size to be used
// RETURNS: pointer to the new shash struct
struct shash* sh_create_map (int size) {
  struct shash* map = malloc(sizeof(struct shash));
  map->_SIZE = size>0 ? size : SASSY_HASH_DEFAULT_SIZE;
  map->_TABLE = malloc(sizeof(struct node*) * map->_SIZE);
  map->CALCULATE_HASH = sh_calculate_hash;
  return map;
}


// 
// sh_delete_map
//
// Deletes the map from memory
void sh_delete_map (struct shash* map) {
  int i;
  for (i = 0; i < map->_SIZE; ++i) {
    ll_delete_list(map->_TABLE[i]); // it checks for null so it's cool
  }
  free(map);
}


// 
// sh_find_node [non API]
// 
// Finds the node with the given 'key' in the given 'map'
// RETURNS: pointer to the node, or NULL if the key does not exist
struct node* sh_find_node (struct shash* map, const char* key) {
  int hash = map->CALCULATE_HASH(map, key);
  struct node* head = map->_TABLE[hash];
  return ll_find_node(head, key);
}

//
// sh_insert
//
// Inserts the 'key' 'value' pair into the given 'map'.
// If the 'key' already exists, replaces the current value with 'value'
// RETURNS: 0 if the key was inserted, 1 if the key already existed
int sh_insert (struct shash* map, const char* key, void* value, int value_size) {
  int hash = map->CALCULATE_HASH(map, key);
  int existed = ll_delete_key(&(map->_TABLE[hash]), key);
  ll_insert_node(&(map->_TABLE[hash]), key, value, value_size);
  return existed;
}

//
// sh_remove
//
// Removes the given 'key' from the given 'map'
// RETURNS: 0 if a key was removed, 1 if the key was not found
int sh_remove (struct shash* map, const char* key) {
  int hash = map->CALCULATE_HASH(map, key);
  return ll_delete_key(&(map->_TABLE[hash]), key); 
}

//
// sh_exists
//
// Checks if a given 'key' exists in the given 'map'
// RETURNS: 1 if it exists, 0 if it does not
int sh_exists (struct shash* map, const char* key) {
  return (sh_find_node(map, key) ? 1 : 0);
}

// 
// sh_get
//
// Gets the value corresponding to the 'key' in the given 'map'
// Places the value into 'value'
// RETURNS: 0 if key was found, 1 if not
int sh_get (struct shash* map, const char* key, void** value) {
  struct node* node = sh_find_node(map, key);
  if (node) *value = node->value;
  return (node ? 0 : 1);
}


//////////////
// PRINTING //
//////////////

//
// sh_print
//
// Prints the map's contents to standard output
// If full != 0, then the buckets will be shown too
void sh_print (struct shash* map, int full) {
  int i;
  char ll [99999];

  if (!full) {
    printf("{");
  }

  int prev = 0;
  for (i = 0; i < map->_SIZE; ++i) {
    ll_to_str(map->_TABLE[i], ll, NULL);
    if (!strlen(ll)) continue;

    if (!full) {
      if (prev) printf(", ");
      prev = 1;
      printf("%s", ll);

    } else {
      printf("%d: \n", i);
      printf("\t%s", ll);
      printf("\n");
    }

  }

  if (!full) {
    printf("}");
  } else {
    printf("\n");
  }
}

