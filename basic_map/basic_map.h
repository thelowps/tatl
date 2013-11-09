#ifndef SASSY_HASH_H
#define SASSY_HASH_H

/*
 * sassyhash.h
 *
 * A simple, yet flexible, thread-safe hashmap.
 *
 * USE: 
 *   Maps strings to arbitrary data (void*).
 *
 * HASH FUNCTION:
 *   Currently, the hash function computes a string's "anagram signature", which is unique
 *   to itself and all its anagrams (lower-case alphabetical only). The hashing will work
 *   with any ASCII string, but there will be less collisions if we deal only with lower-case
 *   alphabetical strings.
 *   Note that for a given map, the hash function can be overwritten. If you do this, make sure
 *   you modulo your hash by map->_SIZE to keep it within the array bounds.
 *
 * COLLISION RESOLUTION:
 *   Collision resolution is done with linked lists at each bucket. Yes, I know it's O(n).
 *   A binary tree would have been too much work. 
 *
 */

struct node;
struct basic_map {
  int _SIZE;
  struct node** _TABLE;
  int (*CALCULATE_HASH) (struct basic_map* map, const char* key);
};

// Create and delete a map
struct basic_map* bm_create_map (int size);
void bm_delete_map ();

// Insert and remove from a map
int  bm_insert (struct basic_map* map, const char* key, void* value, int value_size);
int  bm_remove (struct basic_map* map, const char* key);

// View contents of a map
int  bm_exists (struct basic_map* map, const char* key);
int  bm_get    (struct basic_map* map, const char* key, void** value);

// Debugging. Set full=1 to visualize the buckets
void bm_print  (struct basic_map* map, int full);

#endif
