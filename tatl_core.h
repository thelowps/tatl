#ifndef TATL_CORE_H
#define TATL_CORE_H

/*
 * tatl_core.h
 *
 */

#include "tatl.h"
#include "eztcp.h"
#include "vegCrypt.h"

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
  LISTENER,
  HEARTBEAT
} MESSAGE_TYPE;

typedef struct {
  char name [TATL_MAX_ROOMNAME_SIZE+1];
} troom;

typedef struct {
  char name [TATL_MAX_USERNAME_SIZE+1];
  unsigned int ip_address;
} tuser;

typedef struct {
  MESSAGE_TYPE type;
  char username [TATL_MAX_USERNAME_SIZE+1];
  char roomname [TATL_MAX_ROOMNAME_SIZE+1];
  unsigned int message_id;
  char message [TATL_MAX_CHAT_SIZE+1];
  int message_size;
  
  tuser members [TATL_MAX_MEMBERS_PER_ROOM+1];
  int amount_members;

  troom rooms [TATL_MAX_ROOM_COUNT+1];
  int amount_rooms;
} tmsg;

int  tatl_receive_protocol (int socket, tmsg* message);
void tatl_send_protocol (int socket, tmsg* message);

void tatl_set_error (const char* error);
void tatl_print_error (const char* msg);

void tatl_print_hex (const void* data, int num_bytes);

#endif
