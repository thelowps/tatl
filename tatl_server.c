/*
 * tatl_server.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "tatl.h"
#include "tatl_core.h"
#include "sassyhash.h"
#include "linked.h"

#define DEBUG

extern TATL_MODE CURRENT_MODE;
extern int TATL_USE_AUTHENTICATION;

int TATL_SOCK = 0;

typedef struct {
  char name [TATL_MAX_USERNAME_SIZE];
  char room [TATL_MAX_ROOMNAME_SIZE];
  char ip_address [20];
  int port;
  int socket;
  int listener_socket;
} userdata;

typedef struct {
  char name [TATL_MAX_ROOMNAME_SIZE];
  struct node* users_head;
} roomdata;

shash_t USER_MAP = NULL;
shash_t ROOM_MAP = NULL;

void tatl_print_userdata (void* value, char* str) {
  userdata* data = (userdata*)value;
  sprintf(str, "[name: %s, room:%s, socket:%d]", data->name, data->room, data->socket);
}

void tatl_print_roomdata (void* value, char* str) {
  roomdata* data = (roomdata*)value;
  char users [1024] = {0};
  struct node* head = data->users_head;
  while (head) {
    strcat(users, head->key);
    strcat(users, ", ");
    head = head->next;
  }
  sprintf(str, "[name:%s, users:%s]", data->name, users);
}

void tatl_set_use_authentication (int use_authentication) {
  TATL_USE_AUTHENTICATION = use_authentication;
}

int tatl_init_server (int port, int flags) {
  if (CURRENT_MODE != NOT_INITIALIZED) {
    tatl_set_error("Already initialized.");
    return -1;
  }
  CURRENT_MODE = SERVER;

  USER_MAP = sh_create_map(0);
  ROOM_MAP = sh_create_map(0);

  ezlisten(&TATL_SOCK, port);
  return 0;
}

void* tatl_handle_new_connection (void* arg);
int tatl_run_server () {
  if (CURRENT_MODE != SERVER) {
    tatl_set_error("Not initialized as server.");
    return -1;
  }
  
  pthread_t thread;
  int conn = 0;
  while (1) {
    if ((conn = ezaccept(TATL_SOCK)) < 0) {
      tatl_set_error("Error accepting new client.");
      return -1;
    }
    // TODO: mutex conn to avoid a race condition with multiple clients at the same time
    pthread_create(&thread, NULL, tatl_handle_new_connection, &conn);
  }

  return 0;
}

// Prototypes for use in the main loop
int tatl_login_user   (const char* username, int socket);
int tatl_create_room  (const char* room, const char* username);
int tatl_move_to_room (const char* room, const char* username);
int tatl_user_chatted (const char* chat, const char* username);
int tatl_send_room_members (const char* username);
int tatl_remove_from_room (const char* username);
int tatl_logout_user  (const char* username);

void tatl_handle_client (int socket, const char* username);
void* tatl_handle_new_connection (void* arg) {
  int socket = *((int*)arg);
  
  MESSAGE_TYPE type;
  char message [TATL_MAX_CHAT_SIZE];
  char username [TATL_MAX_USERNAME_SIZE];
  int message_size;

  message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);
  strcpy(username, message);
  
  // LOGIN REQUEST
  if (type == LOGIN) {
    while (1) { // Receive a username until we log in succesfully
#ifdef DEBUG
      printf("Attempting to login a user.\n");
#endif

      if (message_size < 0) {
#ifdef DEBUG
	printf("Failed to log in new user.\n");
#endif
	ezclose(socket);
	return NULL;
      } else if (type != LOGIN) {
	char* err = "Login request with username is required.";
	tatl_send(socket, DENIAL, err, strlen(err)+1);
	continue;
      }
      
      strcpy(username, message);    
      if (tatl_login_user(username, socket)) {
	break; // log in successful
      }
      
      // Receive another username 
      message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);
    }
    
    tatl_handle_client(socket, username);
  }

  // LISTENER REQUEST
  else if (type == LISTENER_REQUEST) {
    userdata user;
    sh_get(USER_MAP, username, &user, sizeof(userdata));
    user.listener_socket = socket;
    sh_set(USER_MAP, username, &user, sizeof(userdata));
  }
  return NULL;
}

// Helper function to create userdata easily
userdata tatl_create_userdata (const char* username, int socket) {
  userdata new_user;
  strncpy(new_user.name, username, TATL_MAX_USERNAME_SIZE);
  new_user.room[0] = 0; //null terminate our string
  new_user.socket = socket;
  new_user.listener_socket = 0;
  ezsocketdata(socket, (char*)&(new_user.ip_address), &(new_user.port));
  return new_user;
}

// Main client handler
void tatl_handle_client (int socket, const char* username) {
  // MAIN CLIENT LOOP
  int message_size;
  char message [TATL_MAX_CHAT_SIZE+1] = {0};
  MESSAGE_TYPE type;
  while (1) {
#ifdef DEBUG
    printf("\n------------------------\nUSER_MAP: \n");
    sh_print(USER_MAP, 0, tatl_print_userdata);
    printf("\n\n");

    printf("ROOM_MAP: \n");
    sh_print(ROOM_MAP, 0, tatl_print_roomdata);
    printf("\n------------------------\n");
#endif

    // Received message
    message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);
#ifdef DEBUG
    printf("Received %d bytes from a user.\n", message_size);
#endif
    if (message_size < 0) {
      tatl_logout_user(username);
      return;
    }

    // Handle message
    // TODO : sanity checks in each function. Should we kill the thread if we log out?
    if (type == CREATE_ROOM_REQUEST) {
      tatl_create_room(message, username);
    } else if (type == ENTER_ROOM_REQUEST) {
      tatl_move_to_room(message, username);
    } else if (type == LEAVE_ROOM_REQUEST) {
      tatl_remove_from_room(username);
    } else if (type == CHAT) {
      tatl_user_chatted(message, username);
    } else if (type == LOGOUT) {
      tatl_logout_user(username);
      return;
    } else if (type == ROOM_MEMBERS_REQUEST) {
      tatl_send_room_members(username);
    }
  }

  return;
}


// Returns 1 on success, 0 on failure
int tatl_login_user (const char* username, int socket) {
  // Check if a user with that name already exists
  if (sh_exists(USER_MAP, username)) {
    char* err = "Username already in use.";
    tatl_send(socket, DENIAL, err, strlen(err)+1);
    return 0;
  }

  // Place the new user in USER_MAP
  userdata new_userdata = tatl_create_userdata(username, socket);
  sh_set(USER_MAP, username, &new_userdata, sizeof(userdata));
  
  // Send a login confirmation
  tatl_send(socket, CONFIRMATION, NULL, 0);
  
#ifdef DEBUG
  printf("%s has logged on. IP: %s, port: %d\n", username, new_userdata.ip_address, new_userdata.port);  
#endif
  return 1;
}


// Returns 1 on success, 0 on failure
int tatl_create_room (const char* roomname, const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));
  
  // Check if the user is already in a room
  if (strlen(user.room) > 0) {
    char* err = "Already in a room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;
  }

  // Check if a room with that name already exists
  if (sh_exists(ROOM_MAP, roomname)) {
    char* err = "A room with that name already exists.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;    
  }

  // We are good to go. Create the room
  roomdata rdata;
  strcpy(rdata.name, roomname);
  rdata.users_head = NULL;
  ll_insert_node(&(rdata.users_head), user.name, NULL, 0);
  sh_set(ROOM_MAP, roomname, &rdata, sizeof(roomdata));
  
  // Change the user's data to indicate which room they are in
  strcpy(user.room, roomname);
  sh_set(USER_MAP, user.name, &user, sizeof(userdata));
  
  // Send a confirmation to the user
  tatl_send(user.socket, CONFIRMATION, NULL, 0);
  
#ifdef DEBUG
  printf("SERVER: created room %s\n", roomname);
#endif 
  return 1;
}


// Place a user into a room
int tatl_move_to_room (const char* roomname, const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  // Check if the user is already in a room
  if (strlen(user.room) > 0) {
    char* err = "Already in a room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;
  }

  // Check if a room with that name exists
  if (!sh_exists(ROOM_MAP, roomname)) {
    char* err = "There is no room with that name.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;    
  }

  // We are good to go. Update the user data first
  strcpy(user.room, roomname);
  sh_set(USER_MAP, user.name, &user, sizeof(userdata));

  // Update the room's data
  roomdata room;
  sh_get(ROOM_MAP, roomname, &room, sizeof(roomdata));
  ll_insert_node(&(room.users_head), user.name, NULL, 0);
  sh_set(ROOM_MAP, roomname, &room, sizeof(roomdata));
  
  // Send a confirmation to the user
  tatl_send(user.socket, CONFIRMATION, NULL, 0);

  return 1;
}

// Send a user's chat to the people in his room
int tatl_user_chatted (const char* chat, const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  // Check if the user is in a room
  if (strlen(user.room) < 0) {
    char* err = "Not in a room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;
  }
  
  // We are good to go. For each user in the chatroom, send the message.
  roomdata room;
  sh_get(ROOM_MAP, user.room, &room, sizeof(roomdata));
  struct node* head = room.users_head;
#ifdef DEBUG
  printf("Sending chat \"%s\"\n", chat);
#endif
  while (head) {
    userdata chatee;
    sh_get(USER_MAP, head->key, &chatee, sizeof(userdata));
    head = head->next;
    if (strcmp(chatee.name, user.name) == 0) continue;
    int bytes_sent = tatl_send(chatee.listener_socket, CHAT, chat, strlen(chat)+1);
#ifdef DEBUG
    printf("Sending chat \"%s\" to %s. Sent %d bytes.\n", chat, chatee.name, bytes_sent);
#endif
  }
  
  return 1;
}

// Send a list of the room members to the given user
int tatl_send_room_members (const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  // Check if the user is in a room
  if (strlen(user.room) < 0) {
    char* err = "Not in a room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;
  }

  // We are good to go. Construct a list of all the room members
  char names [(TATL_MAX_USERNAME_SIZE+1)*TATL_MAX_MEMBERS_PER_ROOM];
  names[0] = 0;

  roomdata room;
  sh_get(ROOM_MAP, user.room, &room, sizeof(roomdata));
  struct node* head = room.users_head;
  while (head) {
    strcat(names, head->key);
    strcat(names, ",");
    head = head->next;
  }

  tatl_send(user.socket, CONFIRMATION, names, strlen(names)+1);

  return 1;
}

// Remove a user from whatever room he is in
int tatl_remove_from_room (const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  // Update room information
  if (strlen(user.room) > 0) {
    roomdata room;
    sh_get(ROOM_MAP, user.room, &room, sizeof(roomdata));
    ll_delete_key(&(room.users_head), username);

    // If the room is now empty, delete it
    if (ll_size(room.users_head) == 0) {
      sh_remove(ROOM_MAP, user.room);
    } else {
      sh_set(ROOM_MAP, user.room, &room, sizeof(roomdata)); 
    }
  }

  // Close user's listener socket
  if (user.listener_socket) {
    ezclose(user.listener_socket);
    user.listener_socket = 0;
  }

  // Update user information
  user.room[0] = 0;
  sh_set(USER_MAP, username, &user, sizeof(userdata));

  return 1;
}

// Destroy a user's data and log him out
int tatl_logout_user (const char* username) {
#ifdef DEBUG
  printf("Logging out user %s\n", username);
#endif
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  tatl_remove_from_room(username);
  ezclose(user.socket);

  sh_remove(USER_MAP, user.name);

  return 1;
}
