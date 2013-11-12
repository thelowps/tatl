#include "tatl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void handle_chat (char* chat) {
  printf("%s\n", chat);
}

int main (int argc, char* argv[]) {
  
  if (argc < 2) {
    printf("Usage: client <username>.\n");
    return 1;
  }

  tatl_init_client (TATL_DEFAULT_SERVER_IP, TATL_DEFAULT_SERVER_PORT, 0);
  if (tatl_login(argv[1])) {
    printf("CLIENT: Error logging in.\n");
    return 1;
  }
  tatl_set_chat_listener(handle_chat);
  
  printf("CLIENT: logged in!\n");

  const char* names [3] = {"three", "four", "seven"};
  
  int i;
  for (i = 0; i < 3; ++i) {
    printf("CLIENT: attempting to log into room %s\n", names[i]);
    if (tatl_request_new_room(names[i])) {
      tatl_enter_room(names[i]);
    }
    
    while (1) {
      char* chat = malloc(sizeof(char) * 1024);
      size_t n = 1024;
      printf("Chat: ");
      int chars = getline(&chat, &n, stdin);
      printf("Read %d characters from input.\n", chars);
      if (chat[chars-1] == '\n') {
	printf("replace\n");
	chat[chars-1] = 0;
      }

      if (strcmp(chat, "quit") == 0) {
	tatl_leave_room();
	break;
      } else {
	tatl_chat(chat);
      }

      free(chat);
    }

  }
  
  return 0;
}
