/* tatl.c
 *
 * Written by David Lopes
 * University of Notre Dame, 2013
 * 
 * Simple chat framework
 */

#include "tatl.h"

#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "eztcp.h"
#include "basic_map.h"

// GLOBALS
enum {
  NOT_INITIALIZED, 
  CLIENT, 
  SERVER
} TATL_MODE = NOT_INITIALIZED;

typedef enum {
  LOGIN, 
  LOGOUT, 
  CREATE_ROOM_REQUEST,
  ENTER_ROOM_REQUEST,
  LEAVE_ROOM_REQUEST,
  AUTHENTICATION_REQUEST,
  AUTHENTICATION_RESPONSE,
  CHAT, 
  CONFIRMATION, 
  DENIAL
} MESSAGE_TYPE;

int TATL_SOCK;
int TATL_USE_AUTHENTICATION = 1;

struct basic_map* USER_MAP = NULL;
struct basic_map* ROOM_MAP = NULL;

// COMMON FUNCTIONS //
int tatl_send (MESSAGE_TYPE message_type, const void* message, int size) {
  ezsend(TATL_SOCK, &message_type, sizeof(int));
  ezsend(TATL_SOCK, &size, sizeof(int));
  if (size > 0) ezsend(TATL_SOCK, message, size);
  return 0;
}

int tatl_receive (MESSAGE_TYPE* message_type, void* message) {
  int message_size = 0;
  ezreceive(TATL_SOCK, &message_type, sizeof(int));
  ezreceive(TATL_SOCK, &message_size, sizeof(int));
  if (message_size > 0) {
    message = malloc(message_size);
    ezreceive(TATL_SOCK, message, message_size);
  }
  return message_size;
}

int tatl_free (void* data) {
  free(data);
  return 0;
}


// CLIENT //
enum {NOT_LOGGED_ON, LOGGED_ON} TATL_CLIENT_STATUS = NOT_LOGGED_ON;

int tatl_init_client (const char* server_ip, int server_port, int flags) {  
  if (ezconnect(&TATL_SOCK, server_ip, server_port) < 0) {
    return -1;
  }
  
  TATL_MODE = CLIENT;
  return 0;
}

int tatl_login (const char* username) {
  // Sanity checking
  if (TATL_MODE != CLIENT) { // Not initialized, or not a client
    return -1;
  } else if (TATL_CLIENT_STATUS == LOGGED_ON) { // Already logged on
    return -1;
  }

  tatl_send(LOGIN, username, strlen(username));
  MESSAGE_TYPE type = DENIAL;
  tatl_receive(&type, NULL);

  if (type == CONFIRMATION) {
    return 0;
  } else {
    return -1;
  }
}


// SERVER //
int tatl_init_server (int port, int flags) {
  if (TATL_MODE != NOT_INITIALIZED) {
    return -1;
  }
  TATL_MODE = SERVER;

  USER_MAP = bm_create_map(0);
  ROOM_MAP = bm_create_map(0);

  ezlisten(&TATL_SOCK, port);
  return 0;
}

void* tatl_handle_client (void* arg);
int tatl_run_server () {
  if (TATL_MODE != SERVER) {
    return -1;
  }
  
  pthread_t thread;
  int conn = 0;
  while (1) {
    if ((conn = ezaccept(TATL_SOCK)) < 0) {
      return -1;
    }
    pthread_create(&thread, NULL, tatl_handle_client, &conn); 
  }

  return 0;
}

void* tatl_handle_client (void* arg) {
  MESSAGE_TYPE type;
  char* message;
  int message_size;

  // Receive a username
  message_size = tatl_receive(&type, message);
  bm_insert(USER_MAP, message, "on", 2);
  printf("%s has logged on.", message);

  // Send a login confirmation
  tatl_send(CONFIRMATION, NULL, 0);

  while (1) {
    message_size = tatl_receive(&type, &message);

    switch (type) {
    case CREATE_ROOM_REQUEST:
      //bm_insert(ROOM_MAP, 
      break;

    case ENTER_ROOM_REQUEST:
      break;

    case LEAVE_ROOM_REQUEST:
      break;

    case CHAT:
      break;

    case LOGOUT:
      break;
    }
  }

  return 0;
}
