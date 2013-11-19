#ifndef TATL_H
#define TATL_H

/* tatl.h
 *
 * Written by David Lopes
 * University of Notre Dame, 2013
 * 
 * Simple chat framework
 */

#define TATL_DEFAULT_SERVER_IP "127.0.0.1"
#define TATL_DEFAULT_SERVER_PORT 9042

#define TATL_MAX_USERNAME_SIZE 100
#define TATL_MAX_ROOMNAME_SIZE 100
#define TATL_MAX_CHAT_SIZE 100
#define TATL_MAX_MEMBERS_PER_ROOM 20
#define TATL_MAX_SERIALIZED_CHAT_SIZE TATL_MAX_USERNAME_SIZE+TATL_MAX_CHAT_SIZE+3

#define TATL_MAX_USERS_IN_ROOM 100

typedef struct {
  char message [TATL_MAX_CHAT_SIZE];
  char sender [TATL_MAX_USERNAME_SIZE];
} tchat;

// CLIENT
int  tatl_init_client (const char* server_ip, int server_port, int flags);
int  tatl_login (const char* username);
int  tatl_request_new_room (const char* roomname);
int  tatl_enter_room (const char* roomname);
int  tatl_request_room_members (char* names);
int  tatl_chat (const char* chat);
int  tatl_leave_room ();

void tatl_set_chat_listener (void (*listen)(char* message));
void tatl_set_authentication_function(int (*fn)(char* gatekeeper));
void tatl_set_gatekeeper_function(int (*fn)(char* knocker));


// SERVER
int tatl_init_server (int port, int flags);
int tatl_run_server ();

// COMMON
void tatl_print_error (const char* msg);

#endif
