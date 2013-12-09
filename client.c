#include "tatl.h"
#include "vegCrypt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void handle_chat (tchat chat) {
  printf("%s> %s\n%s>", chat.sender, chat.message, chat.roomname);
  //printf("%s> ", chat.roomname);
  fflush(stdout);
}

int get_user_input (char* inp, size_t n, const char* prompt) {
  printf("%s", prompt);
  int chars = getline(&inp, &n, stdin);
  if (inp[chars-1] == '\n') {
    inp[chars-1] = 0;
  }
  return chars;
}

//int main (int argc, char* argv[]) {
int main (int argc, char** argv) {
  
  size_t n = 2056;
  char* input = malloc(sizeof(char) * n);
  char list[5]= "list";
  char join[5]= "join";
  char quit[5] = "quit";
  char leave[7] = "/leave";
  char line[4096];
  char * command; 
  int nwords = 0;
  char *words[100];

  char room_data [2056], rooms[2056] = {0}, members[2056] = {0};
  char roomname [TATL_MAX_ROOMNAME_SIZE];
  char username [TATL_MAX_USERNAME_SIZE];

  if (argc < 2){ //if an IP address isn't supplied
    printf("Please specific a server IP address and port\n");
    printf("usage: ./p2pchat [hostname]:[port]\n");
    exit(1);
  }


  //get command line arguments
  char* host_and_port; 
  host_and_port = argv[1];
 
  char host[30];
  char port[10]; 

  strcpy(host, strtok(host_and_port, ":"));
  strcpy(port, strtok(NULL, ":"));
  //
  

  tatl_init_client(host, port, 0);
  tatl_set_chat_listener(handle_chat);
  
  printf("-- Welcome to the most basic of chat clients.   --\n");
  printf("-- Should you find anything not to your liking, --\n");
  printf("-- please send your complaints to /dev/null.    --\n\n");



  printf("-- type 'list' to get a list of existing chatrooms   --\n");
  printf("-- to join, type 'join' [roomname] [username]   --\n");
  printf("-- once in a chatroom, type 'leave' to leave   --\n");
  printf("-- type 'quit' to exit this chat client  --\n\n");


  

  for(;;) {
    line[0] = '\n';
    char * current = "";
    nwords = 0;

    while(line[0] == '\n'){ //to handle user hitting enter and not typing a command ORIGINAL LOOP
      printf("TATLChat> ");
      if(fgets(line, sizeof(line), stdin) == NULL) { //EOF 
	printf("\r");
	exit(0);
      }
    } 

    command = strtok(line," \t\n"); //get just the first word

    while(current != NULL){
      if(nwords!=0){
	words[nwords-1] = current; 
      }
      current = strtok(0, " \t\n");
      nwords++;
    }

    if (strcmp(command, quit) == 0 ){ 
      exit(0);

    }

    if (strcmp(command, list) == 0) { //when you type list repeatedly it multiplies the room names

      memset(rooms, 0, sizeof(rooms));
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

    }

    if(strcmp(command, join) == 0) {
      if(nwords != 3) {
	printf("Please specify a chatroom name and username\n");
		
      }
      else {

	// TODO: not allow inputs with spaces
	strcpy(roomname, words[0]);
	strcpy(username, words[1]);

	if (tatl_join_room(roomname, username, members) < 0) {
	  tatl_print_error("-- Could not enter room");
	} else {
	  printf("-- You have succesfully entered the room.\n");
	  printf("-- Room members are: %s\n", members);
	  printf("-- The floor is yours.\n");
	  printf("-- type '/leave' to leave this chatroom\n\n");
	  printf("%s> ", roomname);
	  while(get_user_input(input, n, "")) {
	    if(strcmp(input, leave) == 0) { //need to supress chat_listener dying error 
	      printf("...leaving room\n");
	      tatl_leave_room (); 
	      break;
	    }

	    tatl_chat(input);
	    printf("%s> ", roomname); 
	  }
	}
      }
    }

  }
  return 0;
}
