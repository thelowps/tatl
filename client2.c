#include "tatl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void handle_chat (char* chat) {
  printf("%s\n", chat);
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
  return 0;
}

int authenticate_other (char* knocker) {
  // tatl_whisper(knocker, "gy")
  // tatl_receive_whisper(knocker, gy);
  // Generate key g^xyh
  // Random number exchange
  // Encrypt room key, send
  // return 0
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
  tatl_init_client (TATL_DEFAULT_SERVER_IP, TATL_DEFAULT_SERVER_PORT, 0);
  
  printf("-- Welcome to the most basic of chat clients.   --\n");
  printf("-- Should you find anything not to your liking, --\n");
  printf("-- please send your complaints to /dev/null.    --\n");
  while(get_user_input(input, n, "-- Enter your desired username: ")) {
    if (tatl_login(input)) {
      tatl_print_error("-- Could not log in");
    } else {
      break;
    }
  }
  
  tatl_set_chat_listener(handle_chat);
  tatl_set_gatekeeper_function(authenticate_other);
  tatl_set_authentication_function(authenticate_self);
  while(get_user_input(input, n, "-- Enter the name of the chatroom you wish to enter: ")) {
    if (tatl_enter_room(input)) {
      tatl_print_error("-- Could not enter room");
      char roomname [1024];
      strcpy(roomname, input);
      get_user_input(input, n, "-- Would you like to create this room? [y/n] ");
      if (strcmp(input, "y") == 0) {
	if (tatl_request_new_room(roomname)) {
	  tatl_print_error("-- Could not create room");
	} else {
	  break;
	}
      }
    } else {
      break;
    }
  }

  printf("-- You have succesfully entered the room.       --\n");
  char room_members [2056];
  tatl_request_room_members(room_members);
  printf("-- Current room members are: %s\n", room_members);
  printf("-- The floor is yours.\n");
  while(get_user_input(input, n, "")) {
    tatl_chat(input);
  }

  return 0;
}
