/*
 * tatl_server.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "tatl_core.h"
#include "sassyhash.h"
#include "linked.h"

extern TATL_MODE CURRENT_MODE;

int TATL_SOCK = 0;

typedef struct {
  char name [TATL_MAX_USERNAME_SIZE];
  char room [TATL_MAX_ROOMNAME_SIZE];
  int socket;
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

int tatl_init_server (int port, int flags) {
  if (CURRENT_MODE != NOT_INITIALIZED) {
    return -1;
  }
  CURRENT_MODE = SERVER;

  USER_MAP = sh_create_map(0);
  ROOM_MAP = sh_create_map(0);

  ezlisten(&TATL_SOCK, port);
  return 0;
}

void* tatl_handle_client (void* arg);
int tatl_run_server () {
  if (CURRENT_MODE != SERVER) {
    return -1;
  }
  
  pthread_t thread;
  int conn = 0;
  while (1) {
    if ((conn = ezaccept(TATL_SOCK)) < 0) {
      return -1;
    }
    pthread_create(&thread, NULL, tatl_handle_client, &conn); 
  }

  return 0;
}

userdata tatl_create_userdata (const char* username, int socket) {
  userdata new_user;
  strncpy(new_user.name, username, TATL_MAX_USERNAME_SIZE);
  new_user.room[0] = 0; //null terminate our string
  new_user.socket = socket;
  return new_user;
}


// Prototypes for use in the main loop
int tatl_login_user   (const char* username, int socket);
int tatl_create_room  (const char* room, const char* username);
int tatl_move_to_room (const char* room, const char* username);
int tatl_user_chatted (const char* chat, const char* username);
int tatl_remove_from_room (const char* username);
int tatl_logout_user  (const char* username);

// Main client handler
void* tatl_handle_client (void* arg) {
  int socket = *((int*)arg);
  
  MESSAGE_TYPE type;
  char* message = malloc(sizeof(char) * TATL_MAX_CHAT_SIZE);
  int message_size;
  char username [TATL_MAX_USERNAME_SIZE];
  
  // LOG THE USER IN
  while (1) { // Receive a username until we log in succesfully
    printf("Attempting to login a user.\n");
    message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);

    if (message_size < 0) {
      printf("Message size < 0. Closing socket.\n");
      ezclose(socket);
      return NULL;
    } else if (type != LOGIN) {
      printf("Error -- incorrect message type.\n");
      char* err = "Please send a login request with your username.";
      tatl_send(socket, DENIAL, err, strlen(err)+1);
      continue;
    }

    strcpy(username, message);    
    if (tatl_login_user(username, socket)) {
      break; // log in successful
    }
  }

  // MAIN CLIENT LOOP
  while (1) {
    printf("\n--SERVER--\nUSER_MAP: \n");
    sh_print(USER_MAP, 0, tatl_print_userdata);
    printf("\n\n");

    printf("ROOM_MAP: \n");
    sh_print(ROOM_MAP, 0, tatl_print_roomdata);
    printf("\n\n");

    message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);
    if (message_size < 0) {
      tatl_logout_user(username);
      return NULL;
    }

    // TODO : Set up an error message system
    switch (type) {
    case CREATE_ROOM_REQUEST:
      tatl_create_room(message, username);
      break;

    case ENTER_ROOM_REQUEST:
      tatl_move_to_room(message, username);
      break;

    case LEAVE_ROOM_REQUEST:
      tatl_remove_from_room(username);
      break;

    case CHAT:
      tatl_user_chatted(message, username);
      break;

    case LOGOUT:
      tatl_logout_user(username);
      return NULL;
    }
  }

  return 0;
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
  
  printf("%s has logged on.\n", username);  
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
  
  printf("SERVER: created room %s\n", roomname);
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
  printf("Sending chat \"%s\"\n", chat);
  while (head) {
    userdata chatee;
    sh_get(USER_MAP, head->key, &chatee, sizeof(userdata));
    head = head->next;
    if (strcmp(chatee.name, user.name) == 0) continue;
    tatl_send(chatee.socket, CHAT, chat, strlen(chat)+1);
    printf("Sending chat \"%s\" to %s\n", chat, chatee.name);
  }
  
  return 1;
}

// Remove a user from whatever room he is in
int tatl_remove_from_room (const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  // Update room information
  roomdata room;
  sh_get(ROOM_MAP, user.room, &room, sizeof(roomdata));
  ll_delete_key(&(room.users_head), username);
  sh_set(ROOM_MAP, user.room, &room, sizeof(roomdata));

  // Update user information
  user.room[0] = 0;
  sh_set(USER_MAP, username, &user, sizeof(userdata));

  // Send a confirmation to the user
  tatl_send(user.socket, CONFIRMATION, NULL, 0);

  return 1;
}

// Destroy a user's data and log him out
int tatl_logout_user (const char* username) {
  // Get updated user data
  userdata user;
  sh_get(USER_MAP, username, &user, sizeof(userdata));

  // Close the user's socket
  ezclose(user.socket);

  // Remove the user from whatever chat rooms he might have been in
  if (strlen(user.room) > 0) {
    roomdata room;
    sh_get(ROOM_MAP, user.room, &room, sizeof(roomdata));
    ll_delete_key(&(room.users_head), user.name);
    sh_set(ROOM_MAP, user.room, &room, sizeof(roomdata));
  }

  // Remove the user from the user map
  sh_remove(USER_MAP, user.name);

  return 1;
}
