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
#include "eztcp.h"

// GLOBALS
enum {NOT_INITIALIZED, CLIENT, SERVER} TATL_MODE = NOT_INITIALIZED;
typedef enum {LOGIN, LOGOUT, CHAT, AUTHENTICATION, CONFIRMATION} MESSAGE_TYPE;
int TATL_SOCK;

// COMMON FUNCTIONS
int tatl_send (MESSAGE_TYPE message_type, const void* message, int size) {
  ezsend(TATL_SOCK, &message_type, sizeof(int));
  ezsend(TATL_SOCK, &size, sizeof(int));
  ezsend(TATL_SOCK, message, size);
  return 0;
}

int tatl_receive (MESSAGE_TYPE* message_type, void* message) {
  int message_size;
  ezreceive(TATL_SOCK, &message_type, sizeof(int));
  ezreceive(TATL_SOCK, &message_size, sizeof(int));
  message = malloc(message_size);
  ezreceive(TATL_SOCK, message, message_size);
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
  MESSAGE_TYPE type;
  int* confirmation = NULL;
  tatl_receive(&type, confirmation);
  int conf = *confirmation;
  tatl_free(confirmation);

  if (conf) {
    return 0;
  } else {
    return -1;
  }
}


// SERVER //
int tatl_init_server (int port, int flags) {
  TATL_MODE = SERVER;

  ezlisten(&TATL_SOCK, port);
  return 0;
}

