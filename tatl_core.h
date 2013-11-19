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
  LOGIN, 
  LOGOUT, 
  CREATE_ROOM_REQUEST,
  ENTER_ROOM_REQUEST,  
  LEAVE_ROOM_REQUEST,
  LISTENER_REQUEST,
  CHAT, 
  ROOM_MEMBERS_REQUEST,
  AUTHENTICATION,
  CONFIRMATION, 
  DENIAL
} MESSAGE_TYPE;

int  tatl_send (int socket, MESSAGE_TYPE message_type, const void* message, int size);
int  tatl_receive (int socket, MESSAGE_TYPE* message_type, void* message, int size);
void tatl_set_error (const char* error);
void tatl_print_error (const char* msg);

void tatl_serialize_chat (char* serialized, tchat chat);
void tatl_deserialize_chat (char* serialized, tchat* chat);

#endif
