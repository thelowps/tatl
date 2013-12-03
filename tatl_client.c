/*
 * tatl_client.c
 *
 */

#include "tatl_core.h"
#include "tatl.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// The socket to communicate with the server on
int TATL_SOCK;
char TATL_SERVER_IP [1024] = {0};
int TATL_SERVER_PORT;

extern TATL_MODE CURRENT_MODE;
typedef enum {IN_ROOM, NOT_IN_ROOM} TATL_CLIENT_STATUS;
TATL_CLIENT_STATUS CURRENT_CLIENT_STATUS = NOT_IN_ROOM;

char CURRENT_USERNAME [TATL_MAX_USERNAME_SIZE] = {0};
char CURRENT_ROOM [TATL_MAX_ROOMNAME_SIZE] = {0};
int  CURRENT_USER_ID = -1;
unsigned int CURRENT_CHAT_ID = 0;

// Function to be called when a chat message from others in the room is received
void (*listener_function)(tchat chat);
pthread_t TATL_LISTENER_THREAD;
void tatl_set_chat_listener (void (*listen)(tchat chat)) {
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
    if (expected_status == NOT_IN_ROOM) tatl_set_error("Already in a room.");
    if (expected_status == IN_ROOM) tatl_set_error("Not in a room.");
    return -1;
  }  
  return 0;
}

int tatl_init_client (const char* server_ip, int server_port, int flags) {  
  if (tatl_sanity_check(NOT_INITIALIZED, NOT_IN_ROOM)) {
    return -1;
  }
  
  if (ezconnect(&TATL_SOCK, server_ip, server_port) < 0) {
    return -1;
  }
  
  // Set flags
  CURRENT_MODE = CLIENT;

  strcpy(TATL_SERVER_IP, server_ip);
  TATL_SERVER_PORT = server_port;

  // Receive ID
  tmsg msg;
  tatl_receive_protocol(TATL_SOCK, &msg);
  sscanf(msg.message, "%d", &CURRENT_USER_ID);  

  return 0;
}

void* tatl_chat_listener (void* arg) {
  int listener_socket;
  ezconnect(&listener_socket, TATL_SERVER_IP, TATL_SERVER_PORT);
  tmsg msg;
  tatl_receive_protocol(listener_socket, &msg); // Consume the standard user id sent, ignore it
  msg.type = LISTENER;
  sprintf(msg.message, "%d", CURRENT_USER_ID);
  tatl_send_protocol(listener_socket, &msg);

  int message_size;
  tchat chat;
  while (1) {  
    message_size = tatl_receive_protocol(listener_socket, &msg);
    if (message_size < 0) {
      pthread_exit(NULL);
    } else if (msg.type == CHAT && listener_function) {
      strcpy(chat.message, msg.message);
      strcpy(chat.sender, msg.username);
      listener_function(chat);	
    }
  }

}


void tatl_spawn_chat_listener () {
  pthread_create(&TATL_LISTENER_THREAD, NULL, tatl_chat_listener, NULL); 
}

void tatl_join_chat_listener () {
  pthread_join(TATL_LISTENER_THREAD, NULL);  
}

// Request entering a room
int tatl_join_room (const char* roomname, const char* username, char* members) {
  if (tatl_sanity_check(CLIENT, NOT_IN_ROOM)) {
    return -1;
  }

  tmsg msg;
  msg.type = JOIN_ROOM;
  strcpy(msg.roomname, roomname);
  strcpy(msg.username, username);
  tatl_send_protocol(TATL_SOCK, &msg);

  tatl_receive_protocol(TATL_SOCK, &msg);
  
  if (msg.type == SUCESS) {
    tatl_spawn_chat_listener();
    CURRENT_CLIENT_STATUS = IN_ROOM;
    strcpy(CURRENT_ROOM, roomname);
    strcpy(members, msg.message);
    return 0;
  } else if (msg.type == AUTHENTICATION) {
    if (authentication_function) {
      //return authentication_function(msg);
      return -1;
    } else {
      return -1;
    }
  } else {
    tatl_set_error(msg.message);
    return -1;
  }  
}
 
// Send a chat to the current chatroom
int tatl_chat (const char* message) {
  if (tatl_sanity_check(CLIENT, IN_ROOM)) return -1;
  
  if (!(*message)) return 0; // Do not send an empty chat
  if (*message == '\n') return 0; // Do not send a single newline

  tmsg msg;
  msg.type = CHAT;
  strcpy(msg.message, message);
  strcpy(msg.username, CURRENT_USERNAME);
  strcpy(msg.username, CURRENT_ROOM);
  msg.message_id = CURRENT_CHAT_ID++;
  tatl_send_protocol(TATL_SOCK, &msg);
  return 0;
}

// Request to leave the current chatroom
int tatl_leave_room () {
  if (tatl_sanity_check(CLIENT, IN_ROOM)) return -1;

  tmsg msg;
  msg.type = LEAVE_ROOM;
  tatl_send_protocol(TATL_SOCK, &msg);

  CURRENT_CLIENT_STATUS = NOT_IN_ROOM;
  CURRENT_ROOM[0] = 0;

  //tatl_join_chat_listener();
  return 0;
}

// Request a list of the rooms
int tatl_request_rooms (char* rooms) {
  if (tatl_sanity_check(CLIENT, NOT_IN_ROOM)) return -1;
  
  tmsg msg;
  msg.type = LIST;
  tatl_send_protocol(TATL_SOCK, &msg);

  tatl_receive_protocol(TATL_SOCK, &msg);
  strcpy(rooms, msg.message);
  return msg.amount_rooms;
}

