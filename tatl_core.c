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
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

TATL_MODE CURRENT_MODE = NOT_INITIALIZED;
char TATL_ERROR [1024] = {0};
int TATL_USE_AUTHENTICATION = 0;

// Print hexadecimal data
void tatl_print_hex (const void* data, int num_bytes) {
  int i;
  for (i = 0; i < num_bytes; ++i) {
    printf("%02x ", ((unsigned char*)data)[i]);
  }
}

// Send a serialized string over the network
int tatl_send (int socket, const char* message, uint32_t msg_size) {
  int bytes_sent = 0;
  uint32_t msg_size_n = htonl(msg_size);
  bytes_sent += ezsend(socket, &msg_size_n, sizeof(msg_size_n));
  printf("raw message size before conversion: %d, after: %d\n", msg_size, msg_size_n);
  bytes_sent += ezsend(socket, message, msg_size);
  printf("Sent %d bytes.\n", bytes_sent);
  //printf("Raw message sent: ");
  //tatl_print_hex(message, msg_size);
  return bytes_sent;
}

// Receive a serialized 
int tatl_receive (int socket, char** message) {
  uint32_t message_size = 0;
  int bytes_received = 0;
  // TODO : send and receive message in network byte order
  if ((bytes_received += ezreceive(socket, &message_size, sizeof(message_size))) <= 0) return -1;
  message_size = ntohl(message_size);
  *message = malloc(message_size);
  memset(*message, message_size, 0);
  if ((bytes_received += ezreceive(socket, *message, message_size)) <= 0) return -1;
  printf("Received %d bytes total.\n", bytes_received);
  //printf("The full raw message received, in hex, is:\n");
  //tatl_print_hex(*message, message_size);
  //printf("\n");
  return message_size;
}

// TODO: explicitly state sizes expected for functions
int tatl_receive_protocol (int socket, tmsg* msg) {
  char* raw_msg;
  char type;
  int raw_msg_size = tatl_receive(socket, &raw_msg);
  if (raw_msg_size < 0) return -1;
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
    sscanf(strtok(NULL, ":"), "%d", &(msg->message_id));
    char* buf = strtok(NULL, ":");
    sscanf(buf, "%d", &(msg->message_size));
    if (msg->message_size) {
      char* cipher = buf + strlen(buf) + 1;
      printf("In receive_protocol. Cipher is: ");
      tatl_print_hex(cipher, msg->message_size);
      memcpy(msg->message, cipher, msg->message_size);
    }
  } else if (type == 'I') {
    msg->type = ID;
    strcpy(msg->message, raw_msg);
  } else if (type == 'N') {
    msg->type = LISTENER;
    strcpy(msg->message, raw_msg);
  } else if (type == 'H') {
    msg->type = HEARTBEAT;
  } else if (type == 'A') { 
    msg->type = AUTHENTICATION;
    char* buf = strtok(raw_msg, ":");
    sscanf(buf, "%u", &(msg->message_size));
    if (msg->message_size) {
      char* cipher = buf + strlen(buf)+1;
      memcpy(msg->message, cipher, msg->message_size);
    }
  }

  free(raw_temp);
  return 0;
}

void tatl_send_protocol (int socket, tmsg* msg) {
  // TODO: sizing
  char* raw_msg = malloc(sizeof(char) * 2056);
  memset(raw_msg, 0, 2056);
  uint32_t msg_size = 0;
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
    sprintf(raw_msg, "T%s:%s:%u:%u:", msg->roomname, msg->username, msg->message_id, msg->message_size);
    msg_size = strlen(raw_msg) + msg->message_size;
    char* temp = raw_msg + strlen(raw_msg);
    memcpy(temp, msg->message, msg->message_size);
    printf("In send_protocol. Set message to: ");
    tatl_print_hex(temp, msg->message_size);
    printf("\n");
  } else if (msg->type == ID) {
    sprintf(raw_msg, "I%s", msg->message);
  } else if (msg->type == LISTENER) {
    sprintf(raw_msg, "N%s", msg->message);
  } else if (msg->type == HEARTBEAT) {
    //sprintf(raw_msg, "H%s:%s", msg->);
  } else if (msg->type == AUTHENTICATION) {
    sprintf(raw_msg, "A%u:", msg->message_size);
    msg_size = strlen(raw_msg) + msg->message_size + 1;
    memcpy(raw_msg+strlen(raw_msg), msg->message, msg->message_size);
  }

  if (msg_size == 0) {
    msg_size = strlen(raw_msg)+1;
  }

  int sent = tatl_send(socket, raw_msg, msg_size);
  if (sent != msg_size+sizeof(msg_size)) {
    printf("WARNING: DID NOT SEND ENTIRE MESSAGE. Expected to send %d, sent %d\n",
	   msg_size+sizeof(msg_size), sent);
  }
  free(raw_msg);
}

void tatl_set_error (const char* error) {
  strcpy(TATL_ERROR, error);
}

void tatl_print_error (const char* msg) {
  printf("%s: %s\n", msg, TATL_ERROR);
}
