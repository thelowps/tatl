/*
 * tatl_client.c
 *
 */

#include "tatl_core.h"
#include "tatl.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <gmp.h>

//#define DEBUG
//#define SEC_DEBUG

// SOCKETS //
int TATL_SERVER_SOCK, TATL_PREV_SOCK, TATL_NEXT_SOCK, TATL_CLIENT_SERVER_SOCK;
char TATL_NEXT_IP[16] = {0}, TATL_PREV_IP[16] = {0}, TATL_SERVER_IP[16] = {0};
int TATL_SERVER_PORT;

// STATE DATA //
extern TATL_MODE CURRENT_MODE;
typedef enum {IN_ROOM, NOT_IN_ROOM} TATL_CLIENT_STATUS;
TATL_CLIENT_STATUS CURRENT_CLIENT_STATUS = NOT_IN_ROOM;

char CURRENT_USERNAME [TATL_MAX_USERNAME_SIZE] = {0};
char CURRENT_ROOM [TATL_MAX_ROOMNAME_SIZE] = {0};
int  CURRENT_USER_ID = -1;
unsigned long CURRENT_CHAT_ID = 0;
int CURRENTLY_IS_HEAD = 0;
int CURRENTLY_PREV_IS_HEAD = 0;
char CURRENT_HEAD_IP [16];
char SELF_IP [16];

// SECURITY //
typedef struct {
  mpz_t p;
  mpz_t gen;
} t_mult_group;

t_mult_group TATL_DEFAULT_MULT_GROUP;

unsigned char CURRENT_AES_KEY [16];
unsigned char CURRENT_MAC_KEY [32];
unsigned char CURRENT_PASS_HASH [32];

// THREADING //
pthread_t TATL_PREV_LISTENER_THREAD, TATL_NEXT_LISTENER_THREAD;
pthread_t TATL_HEARTBEAT_THREAD;
pthread_t TATL_CLIENT_SERVER_THREAD;


// Function to be called when a chat message from others in the room is received
void (*listener_function)(tchat chat);
void tatl_set_chat_listener (void (*listen)(tchat chat)) {
  listener_function = listen;
}

// Helper function to make sure everything is properly set up before performing actions
int tatl_sanity_check (TATL_MODE expected_mode, TATL_CLIENT_STATUS expected_status) {
  if (CURRENT_MODE != expected_mode) {
    if (expected_mode == CLIENT) tatl_set_error("Tatl not initialized as client.");
    if (expected_mode == NOT_INITIALIZED) tatl_set_error("Tatl already initialized.");
    return -1;
  } else if (CURRENT_CLIENT_STATUS != expected_status) {
    if (expected_status == NOT_IN_ROOM) tatl_set_error("Already in a room.");
    if (expected_status == IN_ROOM) tatl_set_error("Not in a room.");
    return -1;
  }  
  return 0;
}

void* tatl_send_heartbeat (void* arg) {
  tmsg msg;
  msg.type = HEARTBEAT;
  while(1) {
    usleep(550000000);
    if (CURRENT_CLIENT_STATUS == IN_ROOM) {
      printf("Sending heartbeat.\n");
      strcpy(msg.roomname, CURRENT_ROOM);
      strcpy(msg.username, CURRENT_USERNAME);
      tatl_send_protocol(TATL_SERVER_SOCK, &msg); 
    }
  }

  return 0;
}

void tatl_spawn_heartbeat_sender() {
  pthread_create(&TATL_HEARTBEAT_THREAD, NULL, tatl_send_heartbeat, NULL); 
}

void* tatl_client_server(void* arg);
int tatl_init_client (const char* server_ip, const char* server_port, int flags) {  
  if (tatl_sanity_check(NOT_INITIALIZED, NOT_IN_ROOM)) {
    return -1;
  }

  if (ezconnect2(&TATL_SERVER_SOCK, server_ip, server_port, TATL_SERVER_IP) < 0) {
    return -1;
  }

  // Set flags
  CURRENT_MODE = CLIENT;
  TATL_SERVER_PORT = atoi(server_port);
  TATL_PREV_SOCK = TATL_NEXT_SOCK = 0;

  // Initialize multiplicative group for diffie-hellman key exchange
  mpz_init(TATL_DEFAULT_MULT_GROUP.gen);
  mpz_init(TATL_DEFAULT_MULT_GROUP.p);
  mpz_set_ui(TATL_DEFAULT_MULT_GROUP.gen, 103);
  mpz_set_str(TATL_DEFAULT_MULT_GROUP.p, "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624225795083", 10);


  // Receive ID
  tmsg msg;
  tatl_receive_protocol(TATL_SERVER_SOCK, &msg);
  CURRENT_USER_ID = msg.message_id;
  strcpy(SELF_IP, msg.message);

  // Spawn the heartbeat sender
  #ifdef DEBUG
  	printf("DEBUG: Spawning heartbeat sender.\n");
  #endif
  tatl_spawn_heartbeat_sender();

  // Create our client server
  pthread_create(&TATL_CLIENT_SERVER_THREAD, NULL, tatl_client_server, NULL);

  return 0;
}

// The step in diffie-hellman that involves making sure the other side 
// has the right key.
int tatl_diffie_hellman_verify_party (int socket, mpz_t gabh, mpz_t inverse, gmp_randstate_t state) {
  int verified = 0;
  tmsg msg;
  msg.type = AUTHENTICATION;
  mpz_t check, resp;
  mpz_init(check);
  mpz_init(resp);

  // Send a random number and expect it +1 back
  mpz_urandomm(check, state, TATL_DEFAULT_MULT_GROUP.p);

  mpz_t expected;
  mpz_init(expected);
  mpz_add_ui(expected, check, 1);

#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Random number to send:\n");
  mpz_out_str(stdout, 10, check);
  printf("\n\n");
#endif  

  // "encrypt" our random number
  mpz_mul(check, check, gabh);
  mpz_mod(check, check, TATL_DEFAULT_MULT_GROUP.p);
#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Encrypted random number:\n");
  mpz_out_str(stdout, 10, check);
  printf("\n\n");
#endif

  // send it
  char* check_str = mpz_get_str(NULL, 10, check);
  strcpy(msg.message, check_str);
  msg.message_size = strlen(check_str)+1;
  free(check_str);
  tatl_send_protocol(socket, &msg);

  // Receive encrypted response
  tatl_receive_protocol(socket, &msg);
  mpz_set_str(resp, msg.message, 10);
#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Received encrypted number: \n%s\n\n", msg.message);
  printf("Expecting number: ");
  mpz_out_str(stdout, 10, expected);
  printf("\n\n");
#endif

  // Decrypt it
  mpz_mul(resp, resp, inverse);
  mpz_mod(resp, resp, TATL_DEFAULT_MULT_GROUP.p);

#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Decrypted response: \n");
  mpz_out_str(stdout, 10, resp);
  printf("\n\n");
#endif

  if (!mpz_cmp(expected, resp)) {
    verified = 1;
  } else {
#ifdef SEC_DEBUG
    printf("VERIFYING OTHER PARTY: The number expected was incorrect.\n\n");
#endif
    verified = 0;
  }
  
#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Verification finished.\n\n");
#endif

  mpz_clear(expected);
  mpz_clear(check);
  mpz_clear(resp);
  return verified;
}

// The step in diffie-hellman that responds to a verification request
// from the other party
void tatl_diffie_hellman_verify_self (int socket, mpz_t gabh, mpz_t inverse) {
  tmsg msg;
  msg.type = AUTHENTICATION;
  mpz_t check, resp;
  mpz_init(check);
  mpz_init(resp);

  // Receive the random number
  tatl_receive_protocol(socket, &msg);
  mpz_set_str(check, msg.message, 10);
#ifdef SEC_DEBUG
  printf("IN SELF VERIFICATION: Received encrypted number:\n%s\n\n", msg.message);
#endif

  mpz_mul(check, check, inverse);
  mpz_mod(check, check, TATL_DEFAULT_MULT_GROUP.p);
#ifdef SEC_DEBUG
  printf("IN SELF VERIFICATION: Decrypted number:\n");
  mpz_out_str(stdout, 10, check);
  printf("\n\n");
#endif

  // Encrypt and return the response * g^abh
  mpz_add_ui(resp, check, 1);    
#ifdef SEC_DEBUG
  printf("IN SELF VERICATION: Added 1 to the number received:\n");
  mpz_out_str(stdout, 10, resp);
  printf("\n\n");
#endif
  mpz_mul(resp, resp, gabh);
  mpz_mod(resp, resp, TATL_DEFAULT_MULT_GROUP.p);

  char* resp_str = mpz_get_str(NULL, 10, resp);
#ifdef SEC_DEBUG
  printf("IN SELF VERIFICATION: Sending response:\n%s\n\n", resp_str);
#endif
  strcpy(msg.message, resp_str);
  msg.message_size = strlen(resp_str)+1;
  free(resp_str);
  tatl_send_protocol(socket, &msg);

  mpz_clear(check);
  mpz_clear(resp);
}

// Authentication function
int tatl_diffie_hellman (int socket, tmsg* initial) {
  tmsg msg;
  msg.type = AUTHENTICATION;

  // Initialize our big numbers
  mpz_t a, ga, gb, gab, pass_hash, gabh;
  mpz_init(a);
  mpz_init(ga);
  mpz_init(gb);
  mpz_init(gab);
  //mpz_init(pass_hash);
  mpz_init(gabh);

  // Generate a random number to seed our RNG
  unsigned long seed = 0;
  FILE* dev_random = fopen("/dev/urandom", "r");
  fread(&seed, sizeof(seed), 1, dev_random);
  fclose(dev_random);

  // Randomly generate our secret number, a
  gmp_randstate_t state;
  gmp_randinit_mt(state);
  gmp_randseed_ui(state, seed);  
  mpz_urandomm(a, state, TATL_DEFAULT_MULT_GROUP.p);

  // Calculate g^a and send it to the other party
  mpz_powm(ga, TATL_DEFAULT_MULT_GROUP.gen, a, TATL_DEFAULT_MULT_GROUP.p);
  char* ga_str = mpz_get_str(NULL, 10, ga);
  strcpy(msg.message, ga_str);
  msg.message_size = strlen(ga_str)+1;
  tatl_send_protocol(socket, &msg);
  free(ga_str);

  // Receive g^b (if we haven't already) and set it
  if (!initial) {
    tatl_receive_protocol(socket, &msg);
    mpz_set_str(gb, msg.message, 10);
  } else {
    mpz_set_str(gb, initial->message, 10);
  }

  // Calculate g^ab
  mpz_powm(gab, gb, a, TATL_DEFAULT_MULT_GROUP.p);

  // Read in our password hash as a number and calculate g^(a*b*hash)
  mpz_import(pass_hash, 32, 1, sizeof(unsigned char), 1, 0, CURRENT_PASS_HASH);  
  //mpz_powm(gabh, gab, pass_hash, TATL_DEFAULT_MULT_GROUP.p);
  mpz_set(gabh, gab);

#ifdef SEC_DEBUG
  printf("Random number for seeding is: %ld\n\n", seed);
  printf("We took our g^b:\n");
  mpz_out_str(stdout, 10, gb);
  printf("\n\nand raised it to a:\n");
  mpz_out_str(stdout, 10, a);
  printf("\n\nand we got g^ab = ");
  mpz_out_str(stdout, 10, gab);
  printf("\n\nand we raised that to our password hash: ");
  mpz_out_str(stdout, 10, pass_hash);
  printf("\n\nwhich, in hex, is: ");
  tatl_print_hex(CURRENT_PASS_HASH, 32);
  printf("\n\nand got our g^abh = ");
  mpz_out_str(stdout, 10, gabh);
  printf("\n\n");
#endif

  // Prepare for our random number handshake
  mpz_t check, resp, gbh, exponent, inverse;
  mpz_init(check);
  mpz_init(resp);
  mpz_init(gbh);
  mpz_init(exponent);
  mpz_init(inverse);

  // calculate gbh
  //mpz_powm(gbh, gb, pass_hash, TATL_DEFAULT_MULT_GROUP.p);
  mpz_set(gbh, gb);

  // Calculate inverse of gabh
  mpz_sub_ui(exponent, TATL_DEFAULT_MULT_GROUP.p, 1);
  mpz_sub(exponent, exponent, a);
  mpz_powm(inverse, gbh, exponent, TATL_DEFAULT_MULT_GROUP.p);

  int success = 0;
#ifdef SEC_DEBUG
  printf("Inverse in decimal before verification: ");
  mpz_out_str(stdout, 10, inverse);
  printf("\n");
#endif
  if (!initial) {
    success = tatl_diffie_hellman_verify_party(socket, gabh, inverse, state);
    tatl_diffie_hellman_verify_self(socket, gabh, inverse);
  } else {
    tatl_diffie_hellman_verify_self(socket, gabh, inverse);
    success = tatl_diffie_hellman_verify_party(socket, gabh, inverse, state);
  }
#ifdef SEC_DEBUG
  printf("Inverse in decimal after verification: ");
  mpz_out_str(stdout, 10, inverse);
  printf("\n");
#endif

  // Free the state
  gmp_randclear(state);

  #ifdef SEC_DEBUG
  	printf("SEC_DEBUG: Verification finished. Success = %d, Prev sock = %d\n", success, TATL_PREV_SOCK);
  #endif
 
 if (success) {
#ifdef SEC_DEBUG
    printf("Handshake successful!\n");
#endif
    mpz_t enc_key, enc_mac_key;
    mpz_init(enc_key);
    mpz_init(enc_mac_key);

    if (!initial) {
#ifdef SEC_DEBUG
      printf("Sending key: ");
      int i;
      for (i = 0; i < 16; ++i) {
	printf("%.2x", CURRENT_AES_KEY[i]);
      }
      printf("\nSending MAC key: ");
      for (i = 0; i < 32; ++i) {
	printf("%.2x", CURRENT_MAC_KEY[i]);
      }
      printf("\n\n");
#endif

      // encrypt the AES key
      mpz_import(enc_key, 16, 1, sizeof(char), 1, 0, CURRENT_AES_KEY);  
#ifdef SEC_DEBUG
      printf("Key sent as decimal: ");
      mpz_out_str(stdout, 10, enc_key);
      printf("\n");
#endif
      mpz_mul(enc_key, enc_key, gabh);
      mpz_mod(enc_key, enc_key, TATL_DEFAULT_MULT_GROUP.p);

      // encrypt the MAC key
      mpz_import(enc_mac_key, 32, 1, sizeof(char), 1, 0, CURRENT_MAC_KEY);
      mpz_mul(enc_mac_key, enc_mac_key, gabh);
      mpz_mod(enc_mac_key, enc_mac_key, TATL_DEFAULT_MULT_GROUP.p);      
      
      // Send the key! yay
      msg.type = AUTHENTICATION;
      char* enc_key_str = mpz_get_str(NULL, 10, enc_key);
      char* enc_mac_key_str = mpz_get_str(NULL, 10, enc_mac_key);
      sprintf(msg.message, "%s:%s", enc_key_str, enc_mac_key_str);
      msg.message_size = strlen(msg.message)+1;
      tatl_send_protocol(socket, &msg);
#ifdef SEC_DEBUG
      printf("Encrypted key sent as decimal: %s\n", enc_key_str);
      printf("Encrypted MAC key sent as decimal: %s\n", enc_mac_key_str);
      
      printf("Decrypting the key, just for checks: ");
      mpz_t dec;
      mpz_init(dec);
      mpz_mul(dec, enc_key, inverse);
      mpz_mod(dec, dec, TATL_DEFAULT_MULT_GROUP.p);
      mpz_out_str(stdout, 10, dec);
      printf("\n\n");
      printf("And the inverse used is: \n");
      mpz_out_str(stdout, 10, inverse);
      printf("\n\n");
#endif
    #ifdef DEBUG
      printf("Freeing. Sock = %d\n", TATL_PREV_SOCK);
#endif
      free(enc_key_str);
      free(enc_mac_key_str);
      
      printf("Key exchange complete.. Prev sock = %d\n", TATL_PREV_SOCK);
      
    } else {
      printf("Receving key. Socket = %d\n", TATL_PREV_SOCK);
      tatl_receive_protocol(socket, &msg);
      printf("Received key. Socket = %d\n", TATL_PREV_SOCK);
      mpz_set_str(enc_key, strtok(msg.message, ":"), 10);
      mpz_set_str(enc_mac_key, strtok(NULL, ":"), 10);
#ifdef SEC_DEBUG
      printf("Received Encrypted key in decimal: \n");
      mpz_out_str(stdout, 10, enc_key);
      printf("\n\n");

      printf("Received Encrypted MAC key in decimal: \n");
      mpz_out_str(stdout, 10, enc_mac_key);
      printf("\n\n");
#endif
      
      size_t produced = 0;
      // Decrypt the AES key
      printf("Decrypting AES key. Sock = %d\n", TATL_PREV_SOCK);
      printf("And the inverse used is: \n");
      mpz_out_str(stdout, 10, inverse);
      printf("\n\n");
      mpz_mul(enc_key, enc_key, inverse);
      mpz_mod(enc_key, enc_key, TATL_DEFAULT_MULT_GROUP.p);
      printf("Exporting AES key. Sock = %d\n", TATL_PREV_SOCK);
      mpz_export(CURRENT_AES_KEY, &produced, 1, sizeof(char), 1, 0, enc_key);     
      printf("Exported AES key. Produced = %zd, Sock = %d\n", produced, TATL_PREV_SOCK);
      
      printf("Decrypting MAC key. Sock = %d\n", TATL_PREV_SOCK);
      // Decrypt the MAC key
      mpz_mul(enc_mac_key, enc_mac_key, inverse);
      mpz_mod(enc_mac_key, enc_mac_key, TATL_DEFAULT_MULT_GROUP.p);
      printf("Exporting MAC key. Sock = %d\n", TATL_PREV_SOCK);
      mpz_export(CURRENT_MAC_KEY, &produced, 1, sizeof(char), 1, 0, enc_mac_key);
      printf("Exported MAC key. Produced: %zd, Sock = %d\n", produced, TATL_PREV_SOCK);
      
#ifdef SEC_DEBUG
      printf("Decrypted key in decimal: \n");
      mpz_out_str(stdout, 10, enc_key);
      printf("\n\n");
      
      printf("Decrypted key in hex: ");
      int i;
      for (i = 0; i < 16; ++i) {
	printf("%.2x", CURRENT_AES_KEY[i]);
      }
      printf("\nDecrypted MAC key in hex: ");
      for (i = 0; i < 32; ++i) {
	printf("%.2x", CURRENT_MAC_KEY[i]);
      }
#endif
      printf("Key exchange complete.. Prev sock = %d\n", TATL_PREV_SOCK);

    } 

    mpz_clear(enc_key);
    mpz_clear(enc_mac_key);
    mpz_clear(check);
    mpz_clear(resp);
    mpz_clear(gbh);
    mpz_clear(exponent);
    mpz_clear(inverse);
    mpz_clear(a);
    mpz_clear(ga);
    mpz_clear(gb);
    mpz_clear(gab);
    mpz_clear(pass_hash);
    mpz_clear(gabh); 

    printf("Exiting diffie hellman. Prev sock = %d\n", TATL_PREV_SOCK);
    return 1;

  } else {
#ifdef SEC_DEBUG
    printf("Handshake unsuccessful.\n\n");
#endif
    mpz_clear(check);
    mpz_clear(resp);
    mpz_clear(gbh);
    mpz_clear(exponent);
    mpz_clear(inverse);
    mpz_clear(a);
    mpz_clear(ga);
    mpz_clear(gb);
    mpz_clear(gab);
    mpz_clear(pass_hash);
    mpz_clear(gabh);

    printf("Freed mpzs.\n");

    return 0;
  }

}

int tatl_recursive_connect (const char* roomname, const char* username);
// Listens for chats on the given socket. Assumes the socket is already set up.
void* tatl_chat_listener (void* arg) {
#ifdef DEBUG
  printf("DEBUG: Starting a chat listener.\n");
#endif
  int* listener_socket = (int*)arg;
  int* forward_socket = NULL;
  forward_socket = *listener_socket == TATL_PREV_SOCK? &TATL_NEXT_SOCK : &TATL_PREV_SOCK;
  
  tmsg msg;
  tchat chat;
  int message_size;
  while (1) {  
    memset(&msg, 0, sizeof(msg));
#ifdef DEBUG
    printf("DEBUG: Waiting for a message on socket: %d", *listener_socket);
#endif
    message_size = tatl_receive_protocol(*listener_socket, &msg);


    // HANDLE CLOSED CONNECTIONS //
#ifdef DEBUG
    printf("DEBUG: Received a message...\n");
#endif
    if (message_size < 0 || msg.type == LEAVE_ROOM) {
#ifdef DEBUG
      printf("Closing a threaded connection.\n");
#endif      
      // check if our main thread is attempting to leave the room.
      // If so, there is nothing more to be done here.
      if (CURRENT_CLIENT_STATUS == NOT_IN_ROOM) {
	#ifdef DEBUG
		printf("DEBUG: Leaving room -- exiting listener thread.\n");
	#endif
	pthread_exit(NULL);
      }

      // If the other side has closed the socket, exit gracefully
      ezclose(*listener_socket);
      *listener_socket = 0;
      if (listener_socket == &TATL_PREV_SOCK) TATL_PREV_IP[0] = 0;
      else if (listener_socket == &TATL_NEXT_SOCK) TATL_NEXT_IP[0] = 0;

      // If we lost our PREV, attempt a new connection to the head.
      if (listener_socket == &TATL_PREV_SOCK) {
	int failure = 0, is_new_head = 0;
	if (strcmp(CURRENT_HEAD_IP, SELF_IP) == 0) {
	  failure = is_new_head = 1;
	} else {
	  failure = is_new_head = ezconnect(&TATL_PREV_SOCK, CURRENT_HEAD_IP, TATL_SERVER_PORT);
	}

	if (!failure) {
	  if (!tatl_recursive_connect(CURRENT_ROOM, CURRENT_USERNAME)) {
	    #ifdef DEBUG
	    	printf("DEBUG: Connected to a new PREV\n");
	    #endif
	  } else {
	    is_new_head = 1;
	  }
	}
	
	if (is_new_head) {
	#ifdef DEBUG
	  	printf("DEBUG: We are the new head! Telling all my friends =)\n");
	#endif
	  if (TATL_NEXT_SOCK) {
	    strcpy(CURRENT_HEAD_IP, SELF_IP);
	    msg.type = HEAD;
	    strcpy(msg.message, CURRENT_HEAD_IP);
	    tatl_send_protocol(TATL_NEXT_SOCK, &msg);
	  } else {
	#ifdef DEBUG
	    printf("We are alone in this world.\n");
	#endif
	  }
	}
      }
       #ifdef DEBUG
      		printf("Exiting listener thread.\n");
	#endif
      pthread_exit(NULL);

      // HANDLE A CHAT //
    } else if (msg.type == CHAT && listener_function) {
      // We have received a chat
#ifdef SEC_DEBUG
      printf("\n\nReceived message from room: %s\nSender: %s\nSize: %d\nCiphertext: ",
	     msg.roomname, msg.username, msg.message_size);
      tatl_print_hex(msg.message, msg.message_size);
      printf("\n");
#endif

      char* plain_text = NULL;      
      int success = deconstructMessage(CURRENT_AES_KEY, CURRENT_MAC_KEY, 
				       &plain_text, (unsigned char*)msg.message);

      if (!success) {
	printf("Received a message with a Bad MAC\n\n");
      }
      strcpy(chat.message, plain_text);
      strcpy(chat.sender, msg.username);
      strcpy(chat.roomname, CURRENT_ROOM);
      listener_function(chat);
      free(plain_text);

      if (*forward_socket) tatl_send_protocol(*forward_socket, &msg);

     
      // HANDLE A HEAD MESSAGE //
    } else if (msg.type == HEAD) {
      strcpy(CURRENT_HEAD_IP, msg.message);
#ifdef DEBUG
      printf("DEBUG: Setting new head IP: %s\n", CURRENT_HEAD_IP);
#endif
      if (*forward_socket) tatl_send_protocol(*forward_socket, &msg);
    }

  }
  
}


void tatl_spawn_chat_listener (int* socket, pthread_t* thread) {
#ifdef DEBUG
  printf("DEBUG: Spawning chat listener with socket: %d", *socket);
#endif
  pthread_create(thread, NULL, tatl_chat_listener, socket); 
}

void tatl_join_chat_listener (pthread_t* thread) {
  pthread_join(*thread, NULL);
}

void* tatl_client_server (void* arg);
// Request entering a room
int tatl_join_room (const char* roomname, const char* username, char* members) {
  if (tatl_sanity_check(CLIENT, NOT_IN_ROOM)) {
    return -1;
  }

  // SEND JOIN REQUEST
  tmsg msg;
  msg.type = JOIN_ROOM;
  strcpy(msg.roomname, roomname);
  strcpy(msg.username, username);
  tatl_send_protocol(TATL_SERVER_SOCK, &msg);

  // RECEIVE RESPONSE FROM SERVER
  tatl_receive_protocol(TATL_SERVER_SOCK, &msg);

  // IF WE ARE NOT ALLOWED TO JOIN, EXIT
  if (msg.type == FAILURE) {
    return -1;
  }

  // TODO : prompt the user for the password
  memset(CURRENT_PASS_HASH, 0, 32);
  hash_pass("b41100ns", CURRENT_PASS_HASH);

  // WE ARE ALLOWED TO JOIN THE ROOM
  int alone = 1;
  int success = 0;
  // If someone is already in the room, connect
  if (*(msg.message)) {
    alone = 0;
#ifdef DEBUG
    printf("DEBUG: Room has someone in it. msg.message is: %s\n", msg.message);
#endif
    char *member, *member_ip;
    member = strtok(msg.message, ":");
    member_ip = strtok(NULL, ":");
    // Find the first person available for connection
    while (1) {
#ifdef DEBUG
      printf("DEBUG: Attempting to connect to %s on ip %s\n", member, member_ip);
#endif
      if (!ezconnect(&TATL_PREV_SOCK, member_ip, TATL_SERVER_PORT)) {
#ifdef DEBUG
	printf("DEBUG: Connected to %s\n", member);
#endif
	success = 1;
	break;
      } else {
#ifdef DEBUG
	printf("DEBUG: Could not connect.\n");
#endif
	member = strtok(NULL, ":");
	if (!member) break;
	member_ip = strtok(NULL, ":");
      }
    }
  }

  int entered_room = 0;
  if (!alone && success) {
    // Find our previous
    int failure = tatl_recursive_connect(roomname, username);
    if (!failure) {
      entered_room = 1;
    } else {
      tatl_set_error("Authentication failure.");
      return -1;
    }

  } else if (alone) {
    entered_room = 1;
    aesKeyGen(CURRENT_AES_KEY);
    macKeyGen(CURRENT_MAC_KEY);
    strcpy(CURRENT_HEAD_IP, SELF_IP);
#ifdef DEBUG
    printf("DEBUG: Setting head ip = %s", CURRENT_HEAD_IP);
#endif
#ifdef SEC_DEBUG
    printf("Generated AES key for room: ");
    tatl_print_hex(CURRENT_AES_KEY, 16);
    printf("\nGenerated MAC key for room: ");
    tatl_print_hex(CURRENT_MAC_KEY, 32);
    printf("\n");
#endif 

  } else if (!alone && !success) {

    tatl_set_error("Could not connect to anyone in the room.\n");
    return -1;
  }

  if (entered_room) {
    CURRENT_CLIENT_STATUS = IN_ROOM;
    strcpy(CURRENT_ROOM, roomname);
    strcpy(CURRENT_USERNAME, username);
    strcpy(members, msg.message);
  }

  return 0;
}

void* tatl_client_server (void* arg) {
  if (ezlisten(&TATL_CLIENT_SERVER_SOCK, TATL_SERVER_PORT)) {
    	perror("Could not set up client server");
    pthread_exit(NULL);
  }

  while (1) {
#ifdef DEBUG
    printf("DEBUG: Client server. Waiting for the worms.\n");
#endif
    int new_connection = ezaccept(TATL_CLIENT_SERVER_SOCK);
    if (new_connection < 0) {
      printf("Error accepting a connection....\n");
      continue;
    }

#ifdef DEBUG
    printf("DEBUG: Opened a connection.\n");
#endif
    // Receive request data
    tmsg msg;
    tatl_receive_protocol(new_connection, &msg);
#ifdef DEBUG
    printf("DEBUG: Request is for room: %s\n", msg.roomname);
    printf("DEBUG: Current room is: %s\n", CURRENT_ROOM);
#endif
    if (!TATL_NEXT_SOCK && !strcmp(CURRENT_ROOM, msg.roomname)) {
#ifdef DEBUG
      printf("DEBUG: Entering diffie_hellman.\n");
#endif
      // We are willing to initiate a NEXT connection, and this request is for 
      // the room that we are in. Good! Authenticate.
      if (tatl_diffie_hellman(new_connection, NULL)) {
#ifdef DEBUG
	printf("DEBUG: Accepted a new NEXT.\n");
#endif
	// Success! We authenticated and have our next.
	TATL_NEXT_SOCK = new_connection;
	ezpeerdata(new_connection, TATL_NEXT_IP, NULL);
#ifdef DEBUG
	printf("DEBUG: NEXT IP is: %s, on socket: %d\n", TATL_NEXT_IP, TATL_NEXT_SOCK);
#endif

	// Inform them about the head
	msg.type = HEAD;
	strcpy(msg.message, CURRENT_HEAD_IP);
	tatl_send_protocol(TATL_NEXT_SOCK, &msg);
#ifdef DEBUG	
	printf("DEBUG: About to spawn a chat listener on socket %d\n", TATL_NEXT_SOCK);
#endif
	tatl_spawn_chat_listener(&TATL_NEXT_SOCK, &TATL_NEXT_LISTENER_THREAD);
      } else {
	printf("Rejected a connection for authentication reasons.\n");
      }
       
    } else {
      // We are already connected to a NEXT. Refer the connection elsewhere
      msg.type = FAILURE;
      strcpy(msg.message, TATL_NEXT_IP);
      tatl_send_protocol(new_connection, &msg);
      ezclose(new_connection);
#ifdef DEBUG
      printf("DEBUG: Rejecting an attempt to connect. Referring to %s\n", TATL_NEXT_IP);
#endif
    }
  }
}

int tatl_recursive_connect (const char* roomname, const char* username) {
  char peer [20];
  int peersock;
  ezpeerdata(TATL_PREV_SOCK, peer, &peersock);
#ifdef DEBUG
  printf("DEBUG: Recursively attempting to connect to ip: %s\n", peer);
  printf("DEBUG: With socket: %d\n", TATL_PREV_SOCK);
#endif

  // Send request
  tmsg msg;
  msg.type = JOIN_ROOM;
  strcpy(msg.roomname, roomname);
  strcpy(msg.username, username);
  tatl_send_protocol(TATL_PREV_SOCK, &msg);
  
  // Receive response
  tatl_receive_protocol(TATL_PREV_SOCK, &msg);
  
  if (msg.type == AUTHENTICATION) {
#ifdef DEBUG
    printf("DEBUG: Authenticating on socket %d\n", TATL_PREV_SOCK);
#endif
    if (tatl_diffie_hellman(TATL_PREV_SOCK, &msg)) {
#ifdef DEBUG
      printf("DEBUG: Success! Socket = %d\n", TATL_PREV_SOCK);
#endif
      tatl_spawn_chat_listener(&TATL_PREV_SOCK, &TATL_PREV_LISTENER_THREAD);
      return 0;
    } else {
#ifdef DEBUG
      printf("DEBUG: Failed to authenticate.\n");
#endif
      return -1;
    }
  } 

  close(TATL_PREV_SOCK);
  TATL_PREV_SOCK = 0;
#ifdef DEBUG
  printf("DEBUG: No dice. Attempting next ip: %s\n", msg.message);
#endif
  if (!(*msg.message)) {
#ifdef DEBUG
    printf("DEBUG: IP referred to us is invalid\n");
#endif
    return -1;
  }

  ezconnect(&TATL_PREV_SOCK, msg.message, TATL_SERVER_PORT);
  return tatl_recursive_connect(roomname, username);
}
 
// Send a chat to the current chatroom
int tatl_chat (char* message) {
#ifdef SEC_DEBUG
  printf("In tatl_chat\n");
#endif
  if (tatl_sanity_check(CLIENT, IN_ROOM)) return -1;
  
  if (!(*message)) return 0; // Do not send an empty chat
  if (*message == '\n') return 0; // Do not send a single newline

  unsigned char cipher [2056];
#ifdef SEC_DEBUG
  printf("Creating full message.\n");
#endif
  long bytes = createMessage(CURRENT_AES_KEY, CURRENT_MAC_KEY, cipher, message);
#ifdef SEC_DEBUG
  printf("Created message.\n");
#endif

  tmsg msg;
  msg.type = CHAT;
  memcpy(msg.message, cipher, bytes);
  msg.message_size = bytes;
  strcpy(msg.username, CURRENT_USERNAME);
  strcpy(msg.roomname, CURRENT_ROOM);
  msg.message_id = CURRENT_CHAT_ID++;

#ifdef SEC_DEBUG
  printf("Sending ciphertext: ");
  tatl_print_hex(msg.message, msg.message_size);
  printf("\n\n");
#endif
  
  if (TATL_PREV_SOCK) tatl_send_protocol(TATL_PREV_SOCK, &msg);
  if (TATL_NEXT_SOCK) tatl_send_protocol(TATL_NEXT_SOCK, &msg);
  return 0;
}

// Request to leave the current chatroom
int tatl_leave_room () {
  if (tatl_sanity_check(CLIENT, IN_ROOM)) return -1;
  
  tmsg msg;
  msg.type = LEAVE_ROOM;
  tatl_send_protocol(TATL_SERVER_SOCK, &msg);

  CURRENT_CLIENT_STATUS = NOT_IN_ROOM;
  CURRENT_ROOM[0] = 0;
  memset(CURRENT_AES_KEY, 0, 16); // erase our keys
  memset(CURRENT_MAC_KEY, 0, 32);
  
  if (TATL_PREV_SOCK) {
#ifdef DEBUG
    printf("DEBUG: Joining previous\n");
#endif
    tatl_send_protocol(TATL_PREV_SOCK, &msg);
    ezclose(TATL_PREV_SOCK);
    TATL_PREV_IP[0] = 0;
    TATL_PREV_SOCK = 0;
    tatl_join_chat_listener(&TATL_PREV_LISTENER_THREAD);
#ifdef DEBUG
    printf("DEBUG: Prev joined.\n");
#endif
  }

  if (TATL_NEXT_SOCK) {
#ifdef DEBUG
    printf("DEBUG: Joining next\n");
#endif
    tatl_send_protocol(TATL_NEXT_SOCK, &msg);
    ezclose(TATL_NEXT_SOCK);
    TATL_NEXT_IP[0] = 0;
    TATL_NEXT_SOCK = 0;
    tatl_join_chat_listener(&TATL_NEXT_LISTENER_THREAD);
#ifdef DEBUG
    printf("DEBUG: Next joined.\n");
#endif
  }

#ifdef DEBUG
  printf("DEBUG: Successfully exited the room.\n");
#endif
  return 0;
}

// Request a list of the rooms
int tatl_request_rooms (char* rooms) {
  if (tatl_sanity_check(CLIENT, NOT_IN_ROOM)) return -1;
  
  tmsg msg;
  msg.type = LIST;
  tatl_send_protocol(TATL_SERVER_SOCK, &msg);

  tatl_receive_protocol(TATL_SERVER_SOCK, &msg);
  strcpy(rooms, msg.message);
  return msg.amount_rooms;
}
