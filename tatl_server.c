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
  userdata* data = *((userdata**)value);
  sprintf(str, "[name: %s, room: %s, socket: %d, ip: %s, port: %d]", 
	  data->name, data->room, data->socket, data->ip_address, data->port);
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

// Entry points to handling userdata
userdata* tatl_create_userdata  (int socket);
userdata* tatl_fetch_userdata   (int socket);
void      tatl_destroy_userdata (int socket);

// Entry points to handling room data
roomdata* tatl_create_roomdata (const char* roomname);
void      tatl_add_user_to_room (roomdata* room, userdata* user);
void      tatl_remove_user_from_room (roomdata* room, userdata* user);
userdata* tatl_get_user_in_room (roomdata* room, const char* username);
void      tatl_destroy_roomdata (roomdata* room);

// Prototypes for use in the main loop
int tatl_place_in_room     (const char* room, userdata* user);
int tatl_remove_from_room  (userdata* user);
int tatl_user_chatted      (const char* chat, userdata* user);
//int tatl_send_room_members (userdata* user);
int tatl_logout_user       (userdata* user);

// Main client handler
void* tatl_handle_new_connection (void* arg) {
  int socket = *((int*)arg);
  userdata* user = tatl_create_userdata(socket);

  // MAIN CLIENT LOOP
  char message [TATL_MAX_CHAT_SIZE+1] = {0};
  MESSAGE_TYPE type;
  while (1) {
    message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);
    
    
#ifdef DEBUG
    printf("\n------------------------\nUSER_MAP: \n");
    sh_print(USER_MAP, 0, tatl_print_userdata);
    printf("\n\n");
    
    printf("ROOM_MAP: \n");
    sh_print(ROOM_MAP, 0, tatl_print_roomdata);
    printf("\n------------------------\n");
    printf("Received %d bytes from a user.\n", message_size);
#endif
    
    
    if (message_size < 0) {
      tatl_logout_user(user);
      return;
    }
    
    // Handle message
    if (type == JOIN_ROOM) {
      tatl_place_in_room(message, user);
    } else if (type == LEAVE_ROOM) {
      tatl_remove_from_room(user);
    } else if (type == CHAT) {
      tatl_user_chatted(message, user);
    } else if (type == ROOMS) {
      //tatl_send_room_members(user);
    } else if (type == LOGOUT) {
      tatl_logout_user(user);
      return;
    } else if (type == LISTENER_REQUEST) {
      user->listener_socket = socket;
    }
  }  
  
  /*  
  // LOGIN REQUEST
  if (type == LOGIN) {
    while (1) { // Receive a username until we log in succesfully
      if (message_size < 0) {
	ezclose(socket);
	return NULL;
      }

      strcpy(username, message);    
      if (tatl_login_user(username, socket)) {
	break; // log in successful
      }
      
      // Receive another username 
      message_size = tatl_receive(socket, &type, message, TATL_MAX_CHAT_SIZE);
    }
     
    tatl_handle_client(socket, username);
    }*/
  
  // LISTENER REQUEST
  return NULL;
}

// Helper function to create userdata easily
userdata* tatl_create_userdata (int socket) {
  userdata* user = malloc(sizeof(userdata));
  user->name[0] = 0;
  user->room[0] = 0;
  user->socket = socket;
  user->listener_socket = 0;
  ezsocketdata(socket, (char*)&(user->ip_address), &(user->port));

  char key [20];
  sprintf(key, "%d", socket);
  sh_set(USER_MAP, key, &user, sizeof(user));
  return user;
}

userdata* tatl_fetch_userdata (int socket) {
  userdata* user = NULL;
  char key [20];
  sprintf(key, "%d", socket);
  sh_get(USER_MAP, key, &user, sizeof(user));
  return user;
}

void tatl_destroy_userdata (int socket) {
  userdata* user = NULL;
  char key [20];
  sprintf(key, "%d", socket);
  sh_get(USER_MAP, key, &user, sizeof(user));
  if (user) {
    sh_remove(USER_MAP, key);
    free(user);  
  }
}

roomdata* tatl_create_roomdata (const char* roomname) {
  roomdata* room = malloc(sizeof(roomdata));
  strcpy(room->name, roomname);
  room->users_head = NULL;
  return room;
}

roomdata* tatl_fetch_roomdata (const char* roomname) {
  roomdata* room = NULL;
  sh_get(ROOM_MAP, roomname, &room, sizeof(room));
  return room;
}

void tatl_add_user_to_room (roomdata* room, userdata* user) {
  ll_insert_node(&(room->users_head), user->name, &user, sizeof(user));
}

void tatl_remove_user_from_room (roomdata* room, userdata* user) {
  ll_delete_key(&(room->users_head), user->name);
}

userdata* tatl_get_user_in_room (roomdata* room, const char* username) {
  struct node* n = ll_find_node(room->users_head, username);
  return *((userdata**)n->value);
}

void tatl_destroy_roomdata (roomdata* room) {
  ll_delete_list(room->users_head);
  free(room);
}


/*
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
    if (type == JOIN_ROOM) {
      tatl_place_in_room(message, username);
    } else if (type == LEAVE_ROOM) {
      tatl_remove_from_room(username);
    } else if (type == CHAT) {
      tatl_user_chatted(message, username);
    } else if (type == ROOM_MEMBERS_REQUEST) {
      tatl_send_room_members(username);
    } else if (type == LOGOUT) {
      tatl_logout_user(username);
      return;
    }
  }  
}
*/

/*
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
*/

// Place a user into a room
int tatl_place_in_room (const char* roomname, userdata* user) {
  // Check if the user is already in a room
  if (strlen(user->room) > 0) {
    char* err = "Already in a room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;
  }

  // Get room data
  roomdata* room;
  if (!(room = tatl_fetch_roomdata(roomname))) {
    room = tatl_create_roomdata(roomname);
  }

  // Check if that name is already in use within that room
  if (tatl_get_user_in_room(room, user->name)) {
    char* err = "Username already in use in that room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);    
    return 0;    
  }

  // Place user in room
  tatl_add_user_to_room(room, user);

  // Update the user data 
  strcpy(user->name, username);
  strcpy(user->room, roomname);

  // Send a confirmation to the user
  tatl_send(user.socket, CONFIRMATION, NULL, 0);

  return 1;
}

// Send a user's chat to the people in his room
int tatl_user_chatted (const char* chat, userdata* user) {
  if (strlen(user->room) <= 0) {
    char* err = "Not in a room.";
    tatl_send(user.socket, DENIAL, err, strlen(err)+1);
    return 0;
  }
  
  // We are good to go. For each user in the chatroom, send the message.
  roomdata* room = tatl_fetch_roomdata(user->room);
  struct node* head = room->users_head;
#ifdef DEBUG
  printf("Sending chat \"%s\"\n", chat);
#endif
  while (head) {
    userdata* chatee = *((userdata**)head->value);
    if (strcmp(chatee->name, user->name) == 0) continue;
    int bytes_sent = tatl_send(chatee->listener_socket, CHAT, chat, strlen(chat)+1);

    head = head->next;
#ifdef DEBUG
    printf("Sending chat \"%s\" to %s. Sent %d bytes.\n", chat, chatee.name, bytes_sent);
#endif
  }
  
  return 1;
}

/*
// Send a list of the room members to the given user
int tatl_send_room_members (userdata* user) {
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
*/

// Remove a user from whatever room he is in
int tatl_remove_from_room (userdata* user) {
  // Update room information
  if (strlen(user->room) > 0) {
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
  if (user->listener_socket) {
    ezclose(user->listener_socket);
    user->listener_socket = 0;
  }

  // Update user information
  user->room[0] = 0;

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
