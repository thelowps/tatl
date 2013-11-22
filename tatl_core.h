#ifndef TATL_CORE_H
#define TATL_CORE_H

/*
 * tatl_core.h
 *
 */

#include "tatl.h"
#include "eztcp.h"

typedef enum {
  NOT_INITIALIZED, 
  CLIENT, 
  SERVER
} TATL_MODE;

typedef enum {
  LIST,
  GROUPS,
  AUTHENTICATION,
  JOIN_ROOM,
  LEAVE_ROOM,
  CHAT,
  LOGOUT,
  SUCCESS,
  FAILURE,
  ID, 
  LISTENER
} MESSAGE_TYPE;

typedef struct {
  char name [TATL_MAX_ROOMNAME_SIZE];
} troom;

typedef struct {
  char name [TATL_MAX_USERNAME_SIZE];
  unsigned int ip_address;
} tuser;

typedef struct {
  MESSAGE_TYPE type;
  char username [TATL_MAX_USERNAME_SIZE];
  char roomname [TATL_MAX_ROOMNAME_SIZE];
  unsigned int message_id;
  char message [TATL_MAX_CHAT_SIZE];
  
  tuser members [TATL_MAX_MEMBERS_PER_ROOM];
  troom rooms [TATL_MAX_ROOM_COUNT];
} tmsg;

int  tatl_receive_protocol (int socket, tmsg* message);
void tatl_send_protocol (int socket, tmsg* message);

void tatl_set_error (const char* error);
void tatl_print_error (const char* msg);

#endif
