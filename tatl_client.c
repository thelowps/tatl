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

// Status of the program
extern TATL_MODE CURRENT_MODE;

// Status of the client
enum {NOT_LOGGED_IN, LOGGED_IN} TATL_CLIENT_STATUS = NOT_LOGGED_IN;

// Function to be called when a chat message from others in the room is received
void (*listener_function)(char* message);

// Thread to listen to chat messages
pthread_t TATL_LISTENER_THREAD;

// Initialize the client
int tatl_init_client (const char* server_ip, int server_port, int flags) {  
  if (ezconnect(&TATL_SOCK, server_ip, server_port) < 0) {
    return -1;
  }
  
  CURRENT_MODE = CLIENT;
  return 0;
}

// Log in to the server with the given username
int tatl_login (const char* username) {
  // Sanity checking
  if (CURRENT_MODE != CLIENT) { // Not initialized, or not a client
    tatl_set_error("Not initialized as client.");
    return -1;
  } else if (TATL_CLIENT_STATUS == LOGGED_IN) { // Already logged on
    tatl_set_error("Already logged on.");
    return -1;
  }

  tatl_send(TATL_SOCK, LOGIN, username, strlen(username)+1);
  char err [1024];
  MESSAGE_TYPE type = DENIAL;
  tatl_receive(TATL_SOCK, &type, err, 1024);

  if (type == CONFIRMATION) {
    TATL_CLIENT_STATUS = LOGGED_IN;
    return 0;
  } else {
    tatl_set_error(err);
    return -1;
  }
}


// Sets the chat listener
void tatl_set_chat_listener (void (*listen)(char* message)) {
  listener_function = listen;
}

// Listen for chats on the given connection and call the listener function
void* tatl_chat_listener (void* arg) {
  MESSAGE_TYPE type;
  char chat [TATL_MAX_CHAT_SIZE];
  int message_size;
  while (1) {
    message_size = tatl_receive(TATL_SOCK, &type, chat, 1024); 
    if (message_size < 0) {
      pthread_exit(NULL);
    } else if (type == CHAT) {
      listener_function(chat);
    } else if (type == CONFIRMATION) {
      pthread_exit(NULL);
    }
  }
  pthread_exit(NULL);
}

// Request creation of a chat room
int tatl_request_new_room (const char* roomname) {
  // Sanity checking
  if (CURRENT_MODE != CLIENT) { // Not initialized, or not a client
    tatl_set_error("Not initialized as client.");
    return -1;
  } else if (TATL_CLIENT_STATUS != LOGGED_IN) { // not logged on
    tatl_set_error("Not logged on.");
    return -1;
  }

  tatl_send(TATL_SOCK, CREATE_ROOM_REQUEST, roomname, strlen(roomname)+1);
  MESSAGE_TYPE type = DENIAL;
  char err [1024];
  tatl_receive(TATL_SOCK, &type, err, 1024);

  if (type == CONFIRMATION) {
    // We are in the room. Spawn the listener thread.
    pthread_create(&TATL_LISTENER_THREAD, NULL, tatl_chat_listener, NULL); 
    return 0;
  } else {
    tatl_set_error(err);
    return -1;
  }
}

// Request entering a room
int tatl_enter_room (const char* roomname) {
  // Sanity checking
  if (CURRENT_MODE != CLIENT) { // Not initialized, or not a client
    tatl_set_error("Not initialized as client.");
    return -1;
  } else if (TATL_CLIENT_STATUS != LOGGED_IN) { // not logged on
    tatl_set_error("Not logged on.");
    return -1;
  }

  tatl_send(TATL_SOCK, ENTER_ROOM_REQUEST, roomname, strlen(roomname)+1);
  MESSAGE_TYPE type = DENIAL;
  char err [1024];
  tatl_receive(TATL_SOCK, &type, err, 1024);
  
  if (type == CONFIRMATION) {
    // We are in the room. Spawn the listener thread.
    pthread_create(&TATL_LISTENER_THREAD, NULL, tatl_chat_listener, NULL); 
    return 0;
  } else {
    tatl_set_error(err);
    return -1;
  }  
}

// Send a chat to the current chatroom
int tatl_chat (const char* chat) {
  // Sanity checking
  if (CURRENT_MODE != CLIENT) { // Not initialized, or not a client
    tatl_set_error("Not initialized as client.");
    return -1;
  } else if (TATL_CLIENT_STATUS != LOGGED_IN) { // not logged on
    tatl_set_error("Not logged on.");
    return -1;
  }

  tatl_send(TATL_SOCK, CHAT, chat, strlen(chat)+1);
  return 0;
}

// Request to leave the current chatroom
int tatl_leave_room () {
  // Sanity checking
  if (CURRENT_MODE != CLIENT) { // Not initialized, or not a client
    tatl_set_error("Not initialized as client.");
    return -1;
  } else if (TATL_CLIENT_STATUS != LOGGED_IN) { // not logged on
    tatl_set_error("Not logged on.");
    return -1;
  }
  
  tatl_send(TATL_SOCK, LEAVE_ROOM_REQUEST, NULL, 0);
  pthread_join(TATL_LISTENER_THREAD, NULL);
  return 0;
}

// Ask for a list of the members of the current chatroom
int tatl_request_room_members () {
  return 1;
}

