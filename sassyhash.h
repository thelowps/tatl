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
 *   you modulo your hash by map->SIZE to keep it within the array bounds.
 *
 * COLLISION RESOLUTION:
 *   Collision resolution is done with linked lists at each bucket. Yes, I know it's O(n). Jesus.
 *
 * NOTES:
 *   Copies of the keys and values are stored internally. There are advantages and disadvantages
 *   to this choice.
 *   Pros: 
 *     You do not have to worry about the scope of the values you pass in as keys or values, since
 *     their scope will always be the same as the scope of the hashmap.
 *     You cannot accidentally tamper with the internal data without an explicit call to sh_set.
 *     Less calls to malloc and free on the user side.
 *   Cons:
 *     Memory copying happens at every get and set. This could get expensive if the data structures 
 *     used as values are big. Use sh_exists as a cheaper check if something exists.
 *     It is not as simple to change the values of existing keys. It must be done with calls to sh_set.
 *
 */

struct shash;
typedef struct shash* shash_t;

// Creates a sassy hashmap and returns a handle to it.
shash_t sh_create_map ();


// Deletes the given sassy hashmap.
void sh_delete_map (shash_t map);


// Inserts into 'map'. Copies 'value_size' bytes stored at 'value'.
// If 'key' already exists, replaces the current value with given 'value'.
int  sh_set (shash_t map, const char* key, void* value, int value_size);


// Copies the value (up to 'max_size') corresponding to 'key' in 'value'.
int  sh_get (shash_t map, const char* key, void* value, int max_size);


// Removes the given 'key' from 'map'.
int  sh_remove (shash_t map, const char* key);


// Returns 1 if 'key' exists in 'map', and 0 otherwise.
int  sh_exists (shash_t map, const char* key);


// Returns the ith element in the hash.
int  sh_at (shash_t map, int i, void* value, int max_size);

// For debugging. If 'full' is not 0, prints a visualization of the buckets each
// key-value pair is in. 'print' is used as the function to print out values (since
// sassyhash is value-type agnostic). If 'print' is null, it assumes the values are 
// strings and simply prints them out as such.
void sh_print  (shash_t map, int full, void (*print)(void* value, char* str));

#endif
