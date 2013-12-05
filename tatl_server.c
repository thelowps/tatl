/*
 * tatl_server.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

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
  shash_t HEARTBEAT_MAP; 
  //struct node* heartbeats_head; // TODO : put this in 
} roomdata;

shash_t USER_MAP = NULL;
shash_t ROOM_MAP = NULL;

void tatl_print_userdata (void* value, char* str) {
  userdata* data = *((userdata**)value);
  sprintf(str, "[name: %s, room: %s, ip: %s, port: %d, socket: %d, listener: %d]", 
	  data->name, data->room, data->ip_address, data->port, data->socket, data->listener_socket);
}

void tatl_print_roomdata (void* value, char* str) {
  roomdata* data = *((roomdata**)value);
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


void* tatl_client_monitor(void * arg);
int tatl_init_server (int port, int flags) {
  if (CURRENT_MODE != NOT_INITIALIZED) {
    tatl_set_error("Already initialized.");
    return -1;
  }
  CURRENT_MODE = SERVER;
  
  USER_MAP = sh_create_map(0);
  ROOM_MAP = sh_create_map(0);

  pthread_t thread;
  
  //create thread that kicks dead clients out every 2 minutes 
  pthread_create(&thread, NULL, tatl_client_monitor, &ROOM_MAP);
  //do I need to call a pthread_join?
  
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
#ifdef DEBUG
      printf("Error accepting new client!\n");
#endif
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
roomdata* tatl_create_roomdata (const char* roomname, const char* username);
roomdata* tatl_fetch_roomdata (const char* roomname);
void      tatl_add_user_to_room (roomdata* room, userdata* user);
void      tatl_remove_user_from_room (roomdata* room, userdata* user);
userdata* tatl_get_user_in_room (roomdata* room, const char* username);
void      tatl_destroy_roomdata (roomdata* room);

// Prototypes for use in the main loop
int tatl_place_in_room     (tmsg* msg, userdata* user);
int tatl_send_rooms        (userdata* user);
int tatl_remove_from_room  (userdata* user);
int tatl_user_chatted      (tmsg* msg, userdata* user);
int tatl_logout_user       (userdata* user);
int tatl_setup_listener    (tmsg* msg, userdata* user);
int tatl_handle_heartbeat  (tmsg* msg);

// Main client handler
void* tatl_handle_new_connection (void* arg) {
  int socket = *((int*)arg);
  userdata* user = tatl_create_userdata(socket);

  tmsg msg;
  msg.type = ID;
  sprintf(msg.message, "%d", socket);
  tatl_send_protocol(socket, &msg);

  // MAIN CLIENT LOOP
  while (1) {
    if (tatl_receive_protocol(socket, &msg) < 0) {
      tatl_logout_user(user);
      return NULL;
    }

#ifdef DEBUG
    printf("\n------------------------\n");
#endif
       
    // Handle message
    if (msg.type == JOIN_ROOM) {
      tatl_place_in_room(&msg, user);
    } else if (msg.type == LEAVE_ROOM) {
      tatl_remove_from_room(user);
    } else if (msg.type == CHAT) {
      tatl_user_chatted(&msg, user);
    } else if (msg.type == LIST) {
      tatl_send_rooms(user);
    } else if (msg.type == LOGOUT) {
      tatl_logout_user(user);
      return NULL;
    } else if (msg.type == LISTENER) {
      tatl_setup_listener(&msg, user);
      return NULL;
    } else if (msg.type == HEARTBEAT) {
	tatl_handle_heartbeat(&msg);
      
    }

#ifdef DEBUG
    printf("\nUSER_MAP: \n");
    sh_print(USER_MAP, 0, tatl_print_userdata);
    printf("\n\n");
    
    printf("ROOM_MAP: \n");
    sh_print(ROOM_MAP, 0, tatl_print_roomdata);
    printf("\n");
#endif
  }  
}

// TODO: standardize error return values
// Place a user into a room
int tatl_place_in_room (tmsg* msg, userdata* user) {
  tmsg resp;

  // Get room data
  roomdata* room;
  if (!(room = tatl_fetch_roomdata(msg->roomname))) {
    room = tatl_create_roomdata(msg->roomname, msg->username);
  }
  
  // Check if that name is already in use within that room
  if (tatl_get_user_in_room(room, msg->username)) {
    resp.type = FAILURE;
    strcpy(resp.message, "Username already in use.");
    tatl_send_protocol(user->socket, &resp);    
    return 0;    
  }

  // Update user data
  strcpy(user->name, msg->username);
  strcpy(user->room, msg->roomname);

  tatl_add_user_to_room(room, user);

  resp.type = SUCCESS;
  resp.message[0] = 0;
  struct node* n = room->users_head;
  while (n) {
    userdata* u = *((userdata**)(n->value));
    strcat(resp.message, u->name);
    strcat(resp.message, ":");
    strcat(resp.message, u->ip_address);
    strcat(resp.message, ":");
    n = n->next;
  }
  tatl_send_protocol(user->socket, &resp);

  return 1;
}

// Send a user's chat to the people in his room
int tatl_user_chatted (tmsg* msg, userdata* user) {
  roomdata* room = tatl_fetch_roomdata(msg->roomname);
  struct node* head = room->users_head;
#ifdef DEBUG
  printf("Sending chat \"%s\"\n", msg->message);
#endif

  tmsg resp;
  resp.type = CHAT;
  strcpy(resp.message, msg->message);
  strcpy(resp.username, user->name);
  strcpy(resp.roomname, user->room);
  while (head) {
    userdata* chatee = *((userdata**)head->value);
    head = head->next;
    if (strcmp(chatee->name, user->name) == 0) continue;
#ifdef DEBUG
    printf("Sending chat \"%s\" to %s\n", msg->message, chatee->name);
#endif    
    tatl_send_protocol(chatee->listener_socket, &resp);
  }

  return 1;
}

// Tell a user which rooms exist
int tatl_send_rooms (userdata* user) {
  tmsg resp;
  resp.type = GROUPS;
  resp.message[0] = 0;

  roomdata* room;
  int i = 0;  
  while (sh_at(ROOM_MAP, i, NULL, &room, sizeof(room))) {
    printf("Adding room %s to list.\n", room->name);
    strcat(resp.message, room->name);
    strcat(resp.message, ":");
    ++i;
  }
  resp.amount_rooms = i;

  printf("Sending full message: %s\n", resp.message);
  tatl_send_protocol(user->socket, &resp);
  return 1;
}

// Remove a user from whatever room he is in
int tatl_remove_from_room (userdata* user) {
  // Update room information
  if (*(user->room)) {
    roomdata* room = tatl_fetch_roomdata(user->room);
    tatl_remove_user_from_room(room, user);
    user->room[0] = 0;

    // If the room is now empty, delete it
    int members_count = ll_size(room->users_head);
    if (members_count == 0) {
      tatl_destroy_roomdata(room);
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
int tatl_logout_user (userdata* user) {
#ifdef DEBUG
  printf("Logging out user %s\n", user->name);
#endif
  tatl_remove_from_room(user);
  ezclose(user->socket);
  
  tatl_destroy_userdata(user->socket);
  
  return 1;
}

// Setup a listener socket for a user 
int tatl_setup_listener (tmsg* msg, userdata* user) {
  int socket;
  int listener_socket = user->socket;
  tatl_destroy_userdata(listener_socket);

  sscanf(msg->message, "%d", &socket);
  user = tatl_fetch_userdata(socket);
  printf("Setting up listener with socket %d for user %s with main socket %d\n", 
	 listener_socket, user->name, user->socket);
  user->listener_socket = listener_socket;
  return 1;
}


// DATA STRUCTURE HELPER FUNCTIONS //
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

roomdata* tatl_create_roomdata (const char* roomname, const char* username) {
  roomdata* room = malloc(sizeof(roomdata));
  shash_t heartbeat_map = sh_create_map();
  strcpy(room->name, roomname);
  room->users_head = NULL;
  sh_set(ROOM_MAP, roomname, &room, sizeof(room));
  room->HEARTBEAT_MAP = heartbeat_map;
  int heartbeat = 1;
  sh_set(room->HEARTBEAT_MAP, username, &heartbeat, sizeof(heartbeat));
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
  user->room[0] = 0;
}

userdata* tatl_get_user_in_room (roomdata* room, const char* username) {
  struct node* n = ll_find_node(room->users_head, username);
  if (!n) return NULL;
  else    return *((userdata**)n->value);
}

void tatl_destroy_roomdata (roomdata* room) {
  ll_delete_list(room->users_head);
  sh_remove(ROOM_MAP, room->name);
  free(room);
}

int tatl_handle_heartbeat(tmsg* msg) {
	roomdata* room = tatl_fetch_roomdata(msg->roomname);
	int heartbeat = 1;
	sh_set(room->HEARTBEAT_MAP, msg->username, &heartbeat, sizeof(heartbeat)); 

return 1;
}

void * tatl_client_monitor(void *arg) {
shash_t ROOM_MAP = *((shash_t*)arg); 


while(1){
	usleep(1200000000); 
	int i = 0;
	int j = 0;
	roomdata* room;
	int heartbeat;
	char* user = malloc(sizeof(TATL_MAX_USERNAME_SIZE));
	int reset = 0;
        

	while(sh_at(ROOM_MAP, i, NULL, &room, sizeof(room))){   
		while(sh_at(room->HEARTBEAT_MAP, j, user, &heartbeat, sizeof(heartbeat))){ 
			if(heartbeat == 0) {
				//have to delete given the key
				sh_remove(room->HEARTBEAT_MAP, user);
			}
			else if(heartbeat == 1) {
				//set it to zero, need the key to set 
				sh_set(room->HEARTBEAT_MAP, user, &reset, sizeof(reset));
			}
			++j;

		}
		++i;
	}
}

return 0;
} 
