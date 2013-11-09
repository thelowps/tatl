#ifndef LINKED_H
#define LINKED_H

/*
 * linked.h
 *
 * Simple doubly linked list to map key-value pairs.
 */

struct node {
  const char* key;
  void* value;
  struct node* next;
  struct node* prev;
};

// Insert into a linked list.
// To create a new one, simply insert into a NULL pointer.
// Changes 'head' to point to the new node inserted (aka the new head of the list)
// 'value_size' is the size (bytes) of the data being used as a value. The linked list
//   maintains internal copies of these values, so make sure it isn't anything huge. If it is,
//   manage the memory yourself and simply store pointers as your values.
void ll_insert_node (struct node** head, const char* key, void* value, int value_size);

// Delete from a linked list.
// Deletes the first node found with the given key.
int  ll_delete_node (struct node** head, const char* key);

// Finds a node in a linked list.
// Returns a pointer to the first node found with the given key.
// Returns NULL if no pointer with the given key was found.
struct node* ll_find_node (struct node* head, const char* key);

// Creates a string representation of the linked list starting at head and stores it in 'str'.
// Note that this is not overflow safe -- make sure 'str' points to enough allocated memory.
// The function pointer is the function to be used to print out the value. It should place the 
// string representation of 'value' into 'str'. If NULL, we assume 'value' is a string and simply 
// print it out. 
void ll_to_str (struct node* head, char* str, void (*tostr)(void* value, char* str));

#endif
