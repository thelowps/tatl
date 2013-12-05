#include "tatl.h"
#include "vegCrypt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void handle_chat (tchat chat) {
  printf("%s: %s\n", chat.sender, chat.message);
}

int authenticate_self (char* gatekeeper) {
  // tatl_whisper(gatekeeper, "gx");
  // tatl_receive_whisper(gatekeeper, gy);
  // Request password from user
  // Calculate hash
  // Generate key g^xyh
  // Encrypt random number, whisper
  // Receive random number+1, decrypt
  // Receive random number, decrypt
  // Encrypt random number+1, whisper
  // Receive room key, decrypt
  // Store room key
  // return 0
  printf("Authenticating self...\n");
  return 0;
}

int authenticate_other (char* knocker) {
  // tatl_whisper(knocker, "gy")
  // tatl_receive_whisper(knocker, gy);
  // Generate key g^xyh
  // Random number exchange
  // Encrypt room key, send
  // return 0
  printf("Authenticating someone else...\n");
  return 0;
}

int get_user_input (char* inp, size_t n, const char* prompt) {
  printf("%s", prompt);
  int chars = getline(&inp, &n, stdin);
  if (inp[chars-1] == '\n') {
    inp[chars-1] = 0;
  }
  return chars;
}

int main (int argc, char* argv[]) {
  
  size_t n = 2056;
  char* input = malloc(sizeof(char) * n);

  tatl_init_client(TATL_DEFAULT_SERVER_IP, TATL_DEFAULT_SERVER_PORT, 0);
  tatl_set_chat_listener(handle_chat);
  tatl_set_gatekeeper_function(authenticate_other);
  tatl_set_authentication_function(authenticate_self);
  
  printf("-- Welcome to the most basic of chat clients.   --\n");
  printf("-- Should you find anything not to your liking, --\n");
  printf("-- please send your complaints to /dev/null.    --\n\n");

  char room_data [2056], rooms[2056] = {0}, members[2056] = {0};
  tatl_request_rooms(room_data);
  char* tok = strtok(room_data, ":");
  if (tok) {
    strcat(rooms, tok);
    while ((tok = strtok(NULL,":"))) {
      strcat(rooms, ", ");
      strcat(rooms, tok);
    }
  }
  printf("-- EXISTING CHATROOMS: %s\n", rooms);


  // TODO: not allow inputs with spaces
  while(1) {
    char roomname [TATL_MAX_ROOMNAME_SIZE];
    get_user_input(input, n, "-- Enter the name of the chatroom you wish to enter: ");
    strcpy(roomname, input);
    get_user_input(input, n, "-- Enter your desired username: ");
    if (tatl_join_room(roomname, input, members) < 0) {
      tatl_print_error("-- Could not enter room");
    } else {
      break;
    }
  }

  printf("-- You have succesfully entered the room.\n");
  printf("-- Room members are: %s\n", members);
  printf("-- The floor is yours.\n");

  while(get_user_input(input, n, "")) {
    tatl_chat(input);
  }

  return 0;
}
