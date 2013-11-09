#include "sassyhash.h"
#include "linked.h"
#include "string.h"

#include <stdio.h>

void int_to_str (void* value, char* str) {
  sprintf(str, "||=~-- %d --~=||", *((int*)value));
}

int main (int argc, char* argv[]) {

  struct node* head = NULL;
  int i;
  for (i = 0; i < 10; ++i) {
    ll_insert_node(&head, "dlopes", &i, sizeof(int));    
  }

  ll_delete_key(&head, "dlopes");
  ll_delete_key(&head, "dlopes");
  ll_delete_key(&head, "dlopes");
  ll_delete_key(&head, "dlopes");
  char str[1024];
  ll_to_str(head, str, int_to_str);
  printf("%s\n\n", str);

  struct shash* map1 = sh_create_map(0);
  struct shash* map2 = sh_create_map(10);
  
  char* v = "my value!";
  sh_insert(map1, "dlopes", v, strlen(v));
  sh_insert(map1, "dovakin", v, strlen(v));
  sh_insert(map1, "bernard", v, strlen(v));
  sh_insert(map1, "dlopes", v, strlen(v));
  sh_insert(map1, "hiya", v, strlen(v));

  sh_print(map1, 1);

  printf("dlopes exists: %s\n", sh_exists(map1, "dlopes") ? "yes" : "no");
  printf("dipshit exists: %s\n", sh_exists(map1, "dipshit") ? "yes" : "no");
  printf("jcobian exists: %s\n", sh_exists(map1, "jcobian") ? "yes" : "no");

  return 0;
}
