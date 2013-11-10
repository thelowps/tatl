/*
 * linked.c
 *
 * Simple doubly linked list.
 * Each node stores a key-value pair.
 */

#include "linked.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

//
// ll_insert_node
//
// Inserts a new node with the given 'key' and 'value' pair at the head of the list starting at 'head'.
// Changes 'head' to point to the new node inserted (aka the new head of the list)
void ll_insert_node (struct node** head, const char* key, void* value, int value_size) {
  struct node* new_node = malloc(sizeof(struct node));

  // Dynamically handle a copy of the key
  char* keycpy = malloc(sizeof(char)*strlen(key) + 1);
  strcpy(keycpy, key);
  new_node->key = keycpy;

  // Dynamically handle a copy of the value
  void* valcpy = malloc(value_size+1); // Just as a precaution. I'd rather insert an extra null byte
  memset(valcpy, 0, value_size+1);     // than struggle with non null-terminated strings that I forget about
  memcpy(valcpy, value, value_size);
  new_node->value = valcpy;

  // Place the new node in the list
  new_node->prev = NULL;
  new_node->next = *head;
  if (*head) (*head)->prev = new_node;
  *head = new_node;
}


//
// ll_delete_key
//
// Deletes the node with the given 'key' in the linked list starting at 'head'
// If the key deleted was the head, the 'head' is changed to be the new head (NULL if empty)
// RETURNS: 0 if a node was deleted, 1 if the node was not found
int ll_delete_key (struct node** head, const char* key) {
  struct node* to_delete = ll_find_node(*head, key);
  if (!to_delete) {
    return 0;
  }

  // If the key is at the head, we need to update the head pointer before deleting
  if (to_delete == *head) {
    *head = (*head)->next;
  }
  
  ll_delete_node(to_delete);
  return 1;
}


//
// ll_delete_node
//
// Deletes the given node of a linked list
void ll_delete_node (struct node* to_delete) {
  // Clear neighboring connections
  if (to_delete->prev) {
    to_delete->prev->next = to_delete->next;
  }

  if (to_delete->next) {
    to_delete->next->prev = to_delete->prev;
  }

  free(to_delete->key);
  free(to_delete->value);
  free(to_delete);
}

//
// ll_delete_list
//
// Deletes entire list starting at 'head'
void ll_delete_list (struct node* head) {
  struct node* next;
  while (head) {
    next = head->next;
    ll_delete_node(head);
    head = next;
  }
}


//
// ll_find_node
//
// Locates the node with 'key' in the linked list starting at 'head'
struct node* ll_find_node (struct node* head, const char* key) {
  for (; head; head = head->next) {
    if (strcmp(head->key, key) == 0) {
      return head;
    }
  }
  return NULL;  
}

 
//
// ll_node_to_str
//
// Places a string representation of 'node' into 'str' up to 'size' characters
void node_to_str (struct node* node, char* str, void (*tostr)(void* value, char* str)) {
  strcpy(str, node->key);
  strcat(str, " => ");
  char buf [2056];
  if (!tostr) {
    sprintf(buf, "\"%s\"", (char*)node->value);
  } else {
    tostr(node->value, buf);
  }
  strcat(str, buf);
}


//
// ll_to_str
//
// Places a string representation of 'head' into 'str'
void ll_to_str (struct node* head, char* str, void (*tostr)(void* value, char* str)) {
  str[0] = 0;
  char buf [1024] = {0};
  while (head) {
    buf[0] = 0;
    node_to_str(head, buf, tostr);
    strncat(str, buf, 1024);
    head = head->next;
    if (head) strncat(str, ", ", 2);
  }
}
