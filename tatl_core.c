/* tatl.c
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

TATL_MODE CURRENT_MODE = NOT_INITIALIZED;
char TATL_ERROR [1024] = {0};

int tatl_send (int socket, MESSAGE_TYPE message_type, const void* message, int size) {
  int bytes_sent = 0;
  bytes_sent += ezsend(socket, &message_type, sizeof(int));
  bytes_sent += ezsend(socket, &size, sizeof(int));
  if (size > 0) bytes_sent += ezsend(socket, message, size);
  return bytes_sent;
}

int tatl_receive (int socket, MESSAGE_TYPE* message_type, void* message, int size) {
  // TODO : make the size parameter actually matter
  int message_size = 0;
  if (ezreceive(socket, message_type, sizeof(int)) <= 0) return -1;
  if (ezreceive(socket, &message_size, sizeof(int)) <= 0) return -1;
  if (message_size > 0) {
    if (ezreceive(socket, message, message_size) <= 0) return -1;
  }
  return message_size;
}

void tatl_set_error (const char* error) {
  strcpy(TATL_ERROR, error);
}

void tatl_print_error (const char* msg) {
  printf("%s: %s\n", msg, TATL_ERROR);
}
