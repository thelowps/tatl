/*
 * sassyhash.c
 *
 *
 */

#include "basic_map.h"
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
// bm_calculate_hash
//
// Calculates the hash for the given key.
// RETURNS: the calculated hash
int bm_calculate_hash (struct basic_map* map, const char* key) {
  long long sig = 1;
  int i = 0;
  for (; i < strlen(key); ++i) {
    sig *= PRIMES[(*key)%26];
  }
  return (sig%map->_SIZE);
}


//
// bm_create_map
//
// Creates a new hash map with the given 'size'. If 0, a default is chosen
// RETURNS: pointer to the new basic_map struct
struct basic_map* bm_create_map (int size) {
  struct basic_map* map = malloc(sizeof(struct basic_map));
  map->_SIZE = size ? size : SASSY_HASH_DEFAULT_SIZE;
  map->_TABLE = malloc(sizeof(struct node*) * map->_SIZE);
  map->CALCULATE_HASH = bm_calculate_hash;
  return map;
}


// 
// bm_delete_map
//
// Deletes the map from memory
void bm_delete_map (struct basic_map* map) {
  free(map);
}


// 
// bm_find_node [non API]
// 
// Finds the node with the given 'key' in the given 'map'
// RETURNS: pointer to the node, or NULL if the key does not exist
struct node* bm_find_node (struct basic_map* map, const char* key) {
  int hash = map->CALCULATE_HASH(map, key);
  struct node* head = map->_TABLE[hash];
  return ll_find_node(head, key);
}

//
// bm_insert
//
// Inserts the 'key' 'value' pair into the given 'map'.
// If the 'key' already exists, replaces the current value with 'value'
// RETURNS: 0 if the key was inserted, 1 if the key already existed
int bm_insert (struct basic_map* map, const char* key, void* value, int value_size) {
  int hash = map->CALCULATE_HASH(map, key);
  int existed = ll_delete_node(&(map->_TABLE[hash]), key);
  ll_insert_node(&(map->_TABLE[hash]), key, value, value_size);
  return existed;
}

//
// bm_remove
//
// Removes the given 'key' from the given 'map'
// RETURNS: 0 if a key was removed, 1 if the key was not found
int bm_remove (struct basic_map* map, const char* key) {
  int hash = map->CALCULATE_HASH(map, key);
  return ll_delete_node(&(map->_TABLE[hash]), key); 
}

//
// bm_exists
//
// Checks if a given 'key' exists in the given 'map'
// RETURNS: 1 if it exists, 0 if it does not
int bm_exists (struct basic_map* map, const char* key) {
  return (bm_find_node(map, key) ? 1 : 0);
}

// 
// bm_get
//
// Gets the value corresponding to the 'key' in the given 'map'
// Places the value into 'value'
// RETURNS: 0 if key was found, 1 if not
int bm_get (struct basic_map* map, const char* key, void** value) {
  struct node* node = bm_find_node(map, key);
  if (node) *value = node->value;
  return (node ? 0 : 1);
}


//////////////
// PRINTING //
//////////////

//
// bm_print
//
// Prints the map's contents to standard output
// If full != 0, then the buckets will be shown too
void bm_print (struct basic_map* map, int full) {
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

