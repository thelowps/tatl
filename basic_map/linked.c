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
  new_node->key = key;
  void* cpy = malloc(value_size);
  memcpy(cpy, value, value_size);
  new_node->value = cpy;
  new_node->prev = NULL;
  new_node->next = *head;
  if (*head) (*head)->prev = new_node;
  *head = new_node;
}


//
// ll_delete_node
//
// Deletes the node with the given 'key' in the linked list starting at 'head'
// If the key deleted was the head, the 'head' is changed to be the new head (NULL if empty)
// RETURNS: 0 if a node was deleted, 1 if the node was not found
int ll_delete_node (struct node** head, const char* key) {
  struct node* to_delete = ll_find_node(*head, key);
  if (!to_delete) {
    return 0;
  }

  if (to_delete->prev) {
    to_delete->prev->next = to_delete->next;
  } else { // we are deleting the head!
    *head = (*head)->next;
  }

  if (to_delete->next) {
    to_delete->next->prev = to_delete->prev;
  }

  free(to_delete->value);
  free(to_delete);
  return 1;
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
