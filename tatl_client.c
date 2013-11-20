/*
 * tatl_client.c
 *
 */

#include "tatl_core.h"
#include "tatl.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// The socket to communicate with the server on
int TATL_SOCK;
char TATL_SERVER_IP [1024] = {0};
int TATL_SERVER_PORT;

extern TATL_MODE CURRENT_MODE;
typedef enum {NOT_LOGGED_IN, LOGGED_IN} TATL_CLIENT_STATUS;
TATL_CLIENT_STATUS CURRENT_CLIENT_STATUS = NOT_LOGGED_IN;

char CURRENT_USERNAME [TATL_MAX_USERNAME_SIZE] = {0};

// Function to be called when a chat message from others in the room is received
void (*listener_function)(char* message);
pthread_t TATL_LISTENER_THREAD;
void tatl_set_chat_listener (void (*listen)(char* message)) {
  listener_function = listen;
}

// Functions to be called when entering a room requires authentication.
// Should return 0 on success, -1 on failure
int (*authentication_function)(char* gatekeeper) = NULL;
int (*gatekeeper_function)(char* knocker) = NULL;
void tatl_set_authentication_function(int (*fn)(char* gatekeeper)) {
  authentication_function = fn;
}
void tatl_set_gatekeeper_function(int (*fn)(char* knocker)) {
  gatekeeper_function = fn;
}

// Helper function to make sure everything is properly set up before performing actions
int tatl_sanity_check (TATL_MODE expected_mode, TATL_CLIENT_STATUS expected_status) {
  if (CURRENT_MODE != expected_mode) {
    if (expected_mode == CLIENT) tatl_set_error("Tatl not initialized as client.");
    if (expected_mode == NOT_INITIALIZED) tatl_set_error("Tatl already initialized.");
    return -1;
  } else if (CURRENT_CLIENT_STATUS != expected_status) {
    if (expected_status == NOT_LOGGED_IN) tatl_set_error("Already logged in.");
    if (expected_status == LOGGED_IN) tatl_set_error("Not logged in.");
    return -1;
  }  
  return 0;
}

int tatl_init_client (const char* server_ip, int server_port, int flags) {  
  if (tatl_sanity_check(NOT_INITIALIZED, NOT_LOGGED_IN)) {
    return -1;
  }
  
  if (ezconnect(&TATL_SOCK, server_ip, server_port) < 0) {
    return -1;
  }
  
  CURRENT_MODE = CLIENT;

  strcpy(TATL_SERVER_IP, server_ip);
  TATL_SERVER_PORT = server_port;

  return 0;
}

/*
int tatl_login (const char* username) {
  if (tatl_sanity_check(CLIENT, NOT_LOGGED_IN)) {
    return -1;
  }

  tatl_send(TATL_SOCK, LOGIN, username, strlen(username)+1);

  char err [1024];
  MESSAGE_TYPE type = DENIAL;
  tatl_receive(TATL_SOCK, &type, err, 1024);

  if (type == CONFIRMATION) {
    strcpy(CURRENT_USERNAME, username);
    CURRENT_CLIENT_STATUS = LOGGED_IN;
    return 0;
  } else {
    tatl_set_error(err);
    return -1;
  }
}
*/

void* tatl_chat_listener (void* arg) {
  int listener_socket;
  ezconnect(&listener_socket, TATL_SERVER_IP, TATL_SERVER_PORT);
  tatl_send(listener_socket, LISTENER, CURRENT_USERNAME, strlen(CURRENT_USERNAME)+1);

  MESSAGE_TYPE type;
  //tchat chat;
  char chat [TATL_MAX_CHAT_SIZE];
  int message_size;
  while (1) {
    message_size = tatl_receive(listener_socket, &type, &chat, 1024); 
    if (message_size < 0) {
      pthread_exit(NULL);
    } else if (type == CHAT) {
      if (listener_function) {
	listener_function(chat);	
      }
    } else if (type == CONFIRMATION) {
      pthread_exit(NULL);
    }
  }
  pthread_exit(NULL);
}


void tatl_spawn_chat_listener () {
  pthread_create(&TATL_LISTENER_THREAD, NULL, tatl_chat_listener, NULL); 
}

void tatl_join_chat_listener () {
  pthread_join(TATL_LISTENER_THREAD, NULL);  
}

// Request entering a room
int tatl_join_room (const char* roomname, const char* username) {
  if (tatl_sanity_check(CLIENT, LOGGED_IN)) {
    return -1;
  }

  int msg_size = strlen(username) + strlen(roomname) + 2;
  char* msg = malloc(sizeof(char)*msg_size);
  memset(msg, 0, msg_size);
  sprintf(msg, "%s:%s", roomname, username);
  tatl_send(TATL_SOCK, JOIN_ROOM, msg, msg_size);

  MESSAGE_TYPE type = DENIAL;
  tatl_receive(TATL_SOCK, &type, msg, 1024);
  
  if (type == CONFIRMATION) {
    tatl_spawn_chat_listener();
    return 0;
  } else if (type == AUTHENTICATION) {
    if (authentication_function) {
      return authentication_function(msg);
    } else {
      return -1;
    }
  } else {
    tatl_set_error(msg);
    return -1;
  }  
}
 


/*
// Request creation of a chat room
int tatl_request_new_room (const char* roomname) {
  if (tatl_sanity_check(CLIENT, LOGGED_IN)) {
    return -1;
  }

  tatl_send(TATL_SOCK, CREATE_ROOM_REQUEST, roomname, strlen(roomname)+1);
  MESSAGE_TYPE type = DENIAL;
  char err [1024];
  tatl_receive(TATL_SOCK, &type, err, 1024);

  if (type == CONFIRMATION) {
    tatl_spawn_chat_listener();
    return 0;
  } else {
    tatl_set_error(err);
    return -1;
  }
}
*/

// Send a chat to the current chatroom
int tatl_chat (const char* message) {
  if (tatl_sanity_check(CLIENT, LOGGED_IN)) {
    return -1;
  }
  
  /*
  tchat chat;
  strcpy(chat.message, message);
  strcpy(chat.sender, CURRENT_USERNAME);
  char serialized [TATL_MAX_SERIALIZED_CHAT_SIZE];
  tatl_serialize_chat(serialized, chat);
  tatl_send(TATL_SOCK, CHAT, serialized, strlen(serialized)+1);
  */
  tatl_send(TATL_SOCK, CHAT, message, strlen(message)+1);
  return 0;
}

// Request to leave the current chatroom
int tatl_leave_room () {
  if (tatl_sanity_check(CLIENT, LOGGED_IN)) {
    return -1;
  }
  
  tatl_send(TATL_SOCK, LEAVE_ROOM, NULL, 0);
  tatl_join_chat_listener();
  return 0;
}

/*
// Ask for a list of the members of the current chatroom
// Returns a list of comma separated names
// TODO : not allow commas in names
int tatl_request_room_members (char* names) {
  if (tatl_sanity_check(CLIENT, LOGGED_IN)) {
    return -1;
  }

  tatl_send(TATL_SOCK, ROOM_MEMBERS_REQUEST, NULL, 0);
  MESSAGE_TYPE type = DENIAL;
  tatl_receive(TATL_SOCK, &type, names, (TATL_MAX_USERNAME_SIZE+1)*TATL_MAX_MEMBERS_PER_ROOM);

  return 0;
}
*/

int tatl_request_rooms (char* rooms) {
  return 0;
}
