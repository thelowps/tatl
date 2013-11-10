/* tatl.c
 *
 * Written by David Lopes
 * University of Notre Dame, 2013
 * 
 * Simple chat framework
 */

#include "tatl.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pthread.h"
#include "eztcp.h"
#include "sassyhash.h"

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

#define MAX_USERNAME_SIZE 100
#define MAX_ROOMNAME_SIZE 100
#define MAX_CHAT_SIZE 100

typedef struct {
  char name [MAX_USERNAME_SIZE];
  char room [MAX_ROOMNAME_SIZE];
  int socket;
} userdata;

shash_t USER_MAP = NULL;
shash_t ROOM_MAP = NULL;

// DEBUGGING //
void tatl_print_userdata (void* value, char* str) {
  userdata* data = (userdata*)value;
  sprintf(str, "{Name=%s : Room=%s : Socket=%d}", data->name, data->room, data->socket);
}

// COMMON FUNCTIONS //
int tatl_send (int socket, MESSAGE_TYPE message_type, const void* message, int size) {
  ezsend(socket, &message_type, sizeof(int));
  ezsend(socket, &size, sizeof(int));
  if (size > 0) ezsend(socket, message, size);
  return 0;
}

int tatl_receive (int socket, MESSAGE_TYPE* message_type, void* message, int size) {
  // TODO : make the size parameter actually matter
  int message_size = 0;
  ezreceive(socket, message_type, sizeof(int));
  ezreceive(socket, &message_size, sizeof(int));
  if (message_size > 0) {
    ezreceive(socket, message, message_size);
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

  tatl_send(TATL_SOCK, LOGIN, username, strlen(username));
  MESSAGE_TYPE type = DENIAL;
  tatl_receive(TATL_SOCK, &type, NULL, 0);

  if (type == CONFIRMATION) {
    TATL_CLIENT_STATUS = LOGGED_ON;
    return 0;
  } else {
    return -1;
  }
}

// Request creation of a chat room
int tatl_create_room (const char* roomname) {
  // Sanity checking
  if (TATL_MODE != CLIENT) { // Not initialized, or not a client
    return -1;
  } else if (TATL_CLIENT_STATUS != LOGGED_ON) { // not logged on
    return -1;
  }

  printf("requesting room %s\n", roomname);
  tatl_send(TATL_SOCK, CREATE_ROOM_REQUEST, roomname, strlen(roomname));
  MESSAGE_TYPE type = DENIAL;
  tatl_receive(TATL_SOCK, &type, NULL, 0); // TODO : received error message
  
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

  USER_MAP = sh_create_map(0);
  ROOM_MAP = sh_create_map(0);

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

userdata tatl_create_userdata (const char* username, int socket) {
  userdata new_user;
  strncpy(new_user.name, username, MAX_USERNAME_SIZE);
  new_user.room[0] = 0; //null terminate our string
  new_user.socket = socket;
  return new_user;
}

void* tatl_handle_client (void* arg) {
  int socket = *((int*)arg);
  
  MESSAGE_TYPE type;
  char* message = malloc(sizeof(char) * MAX_CHAT_SIZE);
  int message_size;
  
  // Receive a username
  message_size = tatl_receive(socket, &type, message, MAX_CHAT_SIZE);
  userdata new_userdata = tatl_create_userdata(message, socket);
  sh_insert(USER_MAP, message, &new_userdata, sizeof(userdata));
  printf("%s has logged on.\n", message);

  // Send a login confirmation
  tatl_send(socket, CONFIRMATION, NULL, 0);
  
  while (1) {
    printf("user map is: \n");
    sh_print(USER_MAP, 0, tatl_print_userdata);
    printf("\n");

    message_size = tatl_receive(socket, &type, message, MAX_CHAT_SIZE);
    if (message_size <= 0) {
      close(socket);
      sh_remove(USER_MAP, new_userdata.name);
      break;
    }
    printf("received message: %s\n", message);

    // TODO : Set up an error message system
    // TODO : redo hash to handle only void pointers, force user to allocate everything himself
    switch (type) {
    case CREATE_ROOM_REQUEST:
      if (!sh_exists(ROOM_MAP, message)) {
	int x = 42;
	sh_insert(ROOM_MAP, message, &x, sizeof(int));
	userdata* data;
	sh_get(USER_MAP, new_userdata.name, &data);
	strcpy(data->room, message);
	printf("SERVER: created room %s\n", (char*)message);

	tatl_send(socket, CONFIRMATION, NULL, 0);
      } else {
	printf("did not create room.\n");
	tatl_send(socket, DENIAL, NULL, 0);;
	// send an error message
      }
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
