#include "tatl.h"
#include <stdio.h>

int get_user_input (char* inp, size_t n, const char* prompt) {
  printf("%s", prompt);
  int chars = getline(&inp, &n, stdin);
  if (inp[chars-1] == '\n') {
    inp[chars-1] = 0;
  }
  return chars;
}

void handle_chat (char* chat) {
  printf("%s\n", chat);
}

int main (int argc, char** argv) {
  
  if (argc < 3) {
    printf("usage: client3 <username> <roomname>");
    return 0;
  }

  tatl_init_client(TATL_DEFAULT_SERVER_IP, TATL_DEFAULT_SERVER_PORT, 0);
  tatl_set_chat_listener(handle_chat);
  tatl_join_room(argv[1], argv[2]);

  char input [TATL_MAX_CHAT_SIZE+1];
  int n = TATL_MAX_CHAT_SIZE+1;
  while(get_user_input(input, n, "")) {
    tatl_chat(input);
  }


  return 0;
}
