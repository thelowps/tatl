/* 
 * tatl_core.c 
 *
 * Written by David Lopes
 * University of Notre Dame, 2013
 * 
 * Simple chat framework
 */

#include "tatl_core.h"
#include "eztcp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

TATL_MODE CURRENT_MODE = NOT_INITIALIZED;
char TATL_ERROR [1024] = {0};
int TATL_USE_AUTHENTICATION = 0;

// Send a null terminated string over the network
int tatl_send (int socket, const char* message) {
  int bytes_sent = 0;
  int msg_size = strlen(message)+1;
  bytes_sent += ezsend(socket, &msg_size, sizeof(int));
  bytes_sent += ezsend(socket, message, msg_size);
  return bytes_sent;
}

// Receive a null-terminated string over the networks
int tatl_receive (int socket, char** message) {
  // TODO : make the size parameter actually matter
  int message_size = 0;
  // TODO : send and receive message in network byte order
  if (ezreceive(socket, &message_size, sizeof(int)) <= 0) return -1;
  *message = malloc(sizeof(char) * message_size);
  if (ezreceive(socket, *message, message_size) <= 0) return -1;
  return message_size;
}

// TODO: explicitly state sizes expected for functions
int tatl_receive_protocol (int socket, tmsg* msg) {
  char* raw_msg;
  char type;
  int msg_size = tatl_receive(socket, &raw_msg);
  if (msg_size < 0) return -1;
  char* raw_temp = raw_msg;
  type = raw_msg[0];
  raw_msg++;
  
  if (type == 'J') {
    msg->type = JOIN_ROOM;
    strcpy(msg->roomname, strtok(raw_msg, ":"));
    strcpy(msg->username, strtok(NULL, ":"));
  } else if (type == 'E') {
    msg->type = LEAVE_ROOM;
  } else if (type == 'S') {
    msg->type = SUCCESS;
    strcpy(msg->message, raw_msg);
  } else if (type == 'F') {
    msg->type = FAILURE;
    strcpy(msg->message, raw_msg);
  } else if (type == 'L') {
    msg->type = LIST;
  } else if (type == 'G') {
    msg->type = GROUPS;
    msg->message[0] = 0;
    sscanf(raw_msg, "%d", &(msg->amount_rooms));
    if (msg->amount_rooms) {
      sscanf(raw_msg, "%d %s", &(msg->amount_rooms), msg->message);
    }
  } else if (type == 'T') {
    msg->type = CHAT;
    strcpy(msg->roomname, strtok(raw_msg, ":"));
    strcpy(msg->username, strtok(NULL, ":"));
    strcpy(msg->message, strtok(NULL, ":"));
  } else if (type == 'I') {
    msg->type = ID;
    strcpy(msg->message, raw_msg);
  } else if (type == 'N') {
    msg->type = LISTENER;
    strcpy(msg->message, raw_msg);
  }
  free(raw_temp);
  return 0;
}

void tatl_send_protocol (int socket, tmsg* msg) {
  // TODO: sizing
  char* raw_msg = malloc(sizeof(char) * 2056);
  if (msg->type == JOIN_ROOM) {
    sprintf(raw_msg, "J%s:%s", msg->roomname, msg->username);
  } else if (msg->type == LEAVE_ROOM) {
    sprintf(raw_msg, "E");
  } else if (msg->type == SUCCESS) {
    sprintf(raw_msg, "S%s", msg->message);
  } else if (msg->type == FAILURE) {
    sprintf(raw_msg, "F%s", msg->message);
  } else if (msg->type == LIST) {
    sprintf(raw_msg, "L");
  } else if (msg->type == GROUPS) {
    sprintf(raw_msg, "G%d %s", msg->amount_rooms, msg->message);
  } else if (msg->type == CHAT) {
    sprintf(raw_msg, "T%s:%s:%s", msg->roomname, msg->username, msg->message);
  } else if (msg->type == ID) {
    sprintf(raw_msg, "I%s", msg->message);
  } else if (msg->type == LISTENER) {
    sprintf(raw_msg, "N%s", msg->message);
  }
  tatl_send(socket, raw_msg);
  free(raw_msg);
}

void tatl_set_error (const char* error) {
  strcpy(TATL_ERROR, error);
}

void tatl_print_error (const char* msg) {
  printf("%s: %s\n", msg, TATL_ERROR);
}
