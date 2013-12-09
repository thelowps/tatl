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

//s STATE DATA //
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
  sscanf(msg.message, "%d", &CURRENT_USER_ID);  

  // Spawn the heartbeat sender
  printf("Spawning heartbeat sender.\n");
  tatl_spawn_heartbeat_sender();

  // Create our client server
  pthread_create(&TATL_CLIENT_SERVER_THREAD, NULL, tatl_client_server, NULL);
  ezsocketdata(TATL_CLIENT_SERVER_SOCK, SELF_IP, NULL);

  return 0;
}

// The step in diffie-hellman that involves making sure the other side 
// has the right key.
int tatl_diffie_hellman_verify_party (int socket, mpz_t gabh, mpz_t inverse, gmp_randstate_t state) {
  tmsg msg;
  msg.type = AUTHENTICATION;
  mpz_t check, resp;
  mpz_init(check);
  mpz_init(resp);

  // Send a random number and expect it +1 back
  mpz_urandomm(check, state, TATL_DEFAULT_MULT_GROUP.p);

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
#endif

  // Decrypt it
  mpz_mul(resp, resp, inverse);
  mpz_mod(resp, resp, TATL_DEFAULT_MULT_GROUP.p);

#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Decrypted response: \n");
  mpz_out_str(stdout, 10, resp);
  printf("\n\n");
#endif

  mpz_add_ui(check, check, 1);
  if (!mpz_cmp(check, resp)) {
#ifdef SEC_DEBUG
    printf("VERIFYING OTHER PARTY: The number expected was incorrect.\n\n");
#endif
    return 0;
  } 

#ifdef SEC_DEBUG
  printf("VERIFYING OTHER PARTY: Party verified.\n\n");
#endif

  mpz_clear(check);
  mpz_clear(resp);
  return 1;
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
  mpz_import(pass_hash, 32, 1, sizeof(char), 1, 0, CURRENT_PASS_HASH);  
  mpz_powm(gabh, gab, pass_hash, TATL_DEFAULT_MULT_GROUP.p);

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
  mpz_powm(gbh, gb, pass_hash, TATL_DEFAULT_MULT_GROUP.p);

  // Calculate inverse of gabh
  mpz_sub_ui(exponent, TATL_DEFAULT_MULT_GROUP.p, 1);
  mpz_sub(exponent, exponent, a);
  mpz_powm(inverse, gbh, exponent, TATL_DEFAULT_MULT_GROUP.p);

  int success = 0;
  if (!initial) {
    success = tatl_diffie_hellman_verify_party(socket, gabh, inverse, state);
    tatl_diffie_hellman_verify_self(socket, gabh, inverse);
  } else {
    tatl_diffie_hellman_verify_self(socket, gabh, inverse);
    success = tatl_diffie_hellman_verify_party(socket, gabh, inverse, state);
  }

  // Free the state
  gmp_randclear(state);

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
#endif
      free(enc_key_str);
      free(enc_mac_key_str);

    } else {
      tatl_receive_protocol(socket, &msg);
      mpz_set_str(enc_key, strtok(msg.message, ":"), 10);
      mpz_set_str(enc_mac_key, strtok(NULL, ":"), 10);
#ifdef SEC_DEBUG
      printf("Received Encrypted key in decimal: \n");
      mpz_out_str(stdout, 10, enc_key);
      printf("\n\n");
#endif

      // Decrypt the AES key
      mpz_mul(enc_key, enc_key, inverse);
      mpz_mod(enc_key, enc_key, TATL_DEFAULT_MULT_GROUP.p);
      mpz_export(CURRENT_AES_KEY, NULL, 1, sizeof(char), 1, 0, enc_key);     

      // Decrypt the MAC key
      mpz_mul(enc_mac_key, enc_mac_key, inverse);
      mpz_mod(enc_mac_key, enc_mac_key, TATL_DEFAULT_MULT_GROUP.p);
      mpz_export(CURRENT_MAC_KEY, NULL, 1, sizeof(char), 1, 0, enc_mac_key);

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

    return 0;
  }

}

int tatl_recursive_connect ();
// Listens for chats on the given socket. Assumes the socket is already set up.
void* tatl_chat_listener (void* arg) {
  printf("Starting a chat listener.\n");
  int* listener_socket = (int*)arg;
  int* forward_socket = NULL;
  forward_socket = *listener_socket == TATL_PREV_SOCK? &TATL_NEXT_SOCK : &TATL_PREV_SOCK;
  
  tmsg msg;
  tchat chat;
  int message_size;
  while (1) {  
    memset(&msg, 0, sizeof(msg));
    message_size = tatl_receive_protocol(*listener_socket, &msg);

    // HANDLE CLOSED CONNECTIONS //
    if (message_size < 0) {
      // If the other side has closed the socket, exit gracefully
      *listener_socket = 0;
       
      // If we lost our PREV, attempt a new connection to the head.
      if (listener_socket == &TATL_PREV_SOCK) {
	int failure = ezconnect(&TATL_PREV_SOCK, CURRENT_HEAD_IP, TATL_SERVER_PORT);
	if (!failure) {
	  if (!tatl_recursive_connect()) {
	    // we are good!
	    printf("Connected to a new PREV\n");
	  } else {
	    // there is something really wrong with the network...
	  }

	} else {
	  printf("We are the new head! Telling all my friends =)\n");
	  if (TATL_NEXT_SOCK) {
	    strcpy(CURRENT_HEAD_IP, SELF_IP);
	    msg.type = HEAD;
	    strcpy(msg.message, CURRENT_HEAD_IP);
	    tatl_send_protocol(TATL_NEXT_SOCK, &msg);
	  } else {
	    printf("We are alone in this world.\n");
	  }
	}
      }
       
      printf("Exiting listener thread.\n");
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
      printf("Received a chat!\n");
      listener_function(chat);
      free(plain_text);

      if (*forward_socket) tatl_send_protocol(*forward_socket, &msg);

     
      // HANDLE A HEAD MESSAGE //
    } else if (msg.type == HEAD) {
      strcpy(CURRENT_HEAD_IP, msg.message);
      printf("Setting new head IP: %s\n", CURRENT_HEAD_IP);
      if (*forward_socket) tatl_send_protocol(*forward_socket, &msg);
    }
  }
  
}


void tatl_spawn_chat_listener (int* socket, pthread_t* thread) {
  pthread_create(thread, NULL, tatl_chat_listener, socket); 
}

void tatl_join_chat_listener (pthread_t* thread) {
  pthread_join(*thread, NULL);
}


int tatl_recursive_connect ();
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
  hash_pass("b41100ns", CURRENT_PASS_HASH);

  // IF WE ARE ALLOWED TO JOIN THE ROOM
  if (msg.type == SUCCESS) {    
    int alone = 1;
    int success = 0;
    // If someone is already in the room, connect
    if (*(msg.message)) {
      alone = 0;

      printf("Room has someone in it. msg.message is: %s\n", msg.message);
      char* member, *member_ip;
      member = strtok(msg.message, ":");
      member_ip = strtok(NULL, ":");

      // Find the first person available for connection
      while (1) {
	printf("Attempting to connect to %s on ip %s\n", member, member_ip);
	if (!ezconnect(&TATL_PREV_SOCK, member_ip, TATL_SERVER_PORT)) {

	  printf("Connected to %s\n", member);
	  success = 1;
	  break;
	} else {
	  printf("Could not connect.\n");
	  member = strtok(NULL, ":");
	  if (!member) break;
	  member_ip = strtok(NULL, ":");
	}
      }
    }

    int entered_room = 0;
    if (!alone && success) {
      // Find our previous
      printf("Initiating a recursive connection!\n");
      int failure = tatl_recursive_connect();
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
      printf("Setting head ip = %s", CURRENT_HEAD_IP);
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

    // Listen for our next
    // TODO : join the thread
    if (entered_room) {
      //if (alone) CURRENTLY_IS_HEAD = 1;
      //else CURRENTLY_IS_HEAD = 0;
      CURRENT_CLIENT_STATUS = IN_ROOM;
      strcpy(CURRENT_ROOM, roomname);
      strcpy(CURRENT_USERNAME, username);
      strcpy(members, msg.message);
    }    

    return 0;
  } else {
    return -1;
  }
}

void* tatl_client_server (void* arg) {
  if (ezlisten(&TATL_CLIENT_SERVER_SOCK, TATL_SERVER_PORT)) {
    perror("Could not set up client server");
    pthread_exit(NULL);
  }

  while (1) {
    printf("Client server. Waiting for the worms.\n");
    int new_connection = ezaccept(TATL_CLIENT_SERVER_SOCK);
    if (new_connection < 0) {
      printf("Error accepting a client.\n");
      continue;
    }
     
    printf("Accepted a client.\n");
    ezpeerdata(new_connection, TATL_NEXT_IP, NULL);
    printf("Dat peerdata tho\n");

    if (!TATL_NEXT_SOCK) {
      // We are willing to initiate a NEXT connection here. Authenticate
      if (tatl_diffie_hellman(new_connection, NULL)) {
	TATL_NEXT_SOCK = new_connection;

	// Inform them about the new head
	tmsg msg;
	msg.type = HEAD;
	strcpy(msg.message, CURRENT_HEAD_IP);
	tatl_send_protocol(TATL_NEXT_SOCK, &msg);
	
	tatl_spawn_chat_listener(&TATL_NEXT_SOCK, &TATL_NEXT_LISTENER_THREAD);
      } else {
	printf("Rejected a connection for authentication reasons.\n");
      }
       
    } else {
      // We are already connected to a NEXT. Refer the connection elsewhere
      tmsg msg;
      msg.type = FAILURE;
      strcpy(msg.message, TATL_NEXT_IP);
      tatl_send_protocol(new_connection, &msg);
      ezclose(new_connection);
    }
  }
}

int tatl_recursive_connect () {
  printf("Recursive connect. Receiving on socket %d\n", TATL_PREV_SOCK);
  char peer [20];
  int peersock;
  ezpeerdata(TATL_PREV_SOCK, peer, &peersock);
  printf("Peer ip is: %s, socket %d\n", peer, peersock);

  tmsg msg;
  tatl_receive_protocol(TATL_PREV_SOCK, &msg);
  
  if (msg.type == AUTHENTICATION) {
    printf("Authenticating...\n");
    if (tatl_diffie_hellman(TATL_PREV_SOCK, &msg)) {
      printf("Success!\n");
      tatl_spawn_chat_listener(&TATL_PREV_SOCK, &TATL_PREV_LISTENER_THREAD);
      return 0;
    } else {
      printf("Failed to authenticate.\n");
      return -1;
    }
  } 

  printf("No dice. Attempting next ip: %s\n", msg.message);
  close(TATL_PREV_SOCK);
  TATL_PREV_SOCK = 0;
  ezconnect(&TATL_PREV_SOCK, msg.message, TATL_SERVER_PORT);
  return tatl_recursive_connect();
}

// Send a chat to the current chatroom
int tatl_chat (const char* message) {
  if (tatl_sanity_check(CLIENT, IN_ROOM)) return -1;
  
  if (!(*message)) return 0; // Do not send an empty chat
  if (*message == '\n') return 0; // Do not send a single newline
  
  char* plain = malloc(2056);
  unsigned char cipher [2056];
  strcpy(plain, message);
  
  long bytes = createMessage(CURRENT_AES_KEY, CURRENT_MAC_KEY, cipher, plain);
  free(plain);

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

  if (TATL_PREV_SOCK) {
    ezclose(TATL_PREV_SOCK);
    tatl_join_chat_listener(&TATL_PREV_LISTENER_THREAD);
  }

  if (TATL_NEXT_SOCK) {
    ezclose(TATL_NEXT_SOCK);
    tatl_join_chat_listener(&TATL_NEXT_LISTENER_THREAD);
  }
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
