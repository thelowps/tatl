#ifndef TATL_CORE_H
#define TATL_CORE_H

/*
 * tatl_core.h
 *
 */

#include "tatl.h"
#include "eztcp.h"

// Defines the mode that a tatl program is running in
typedef enum {
  NOT_INITIALIZED, 
  CLIENT, 
  SERVER
} TATL_MODE;

// Defines types of messages, used for sending and receiving
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

int  tatl_send (int socket, MESSAGE_TYPE message_type, const void* message, int size);
int  tatl_receive (int socket, MESSAGE_TYPE* message_type, void* message, int size);
void tatl_set_error (const char* error);
void tatl_print_error (const char* msg);

#endif
