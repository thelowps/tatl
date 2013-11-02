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

// CLIENT
int tatl_init_client (const char* server_ip, int server_port, int flags);
int tatl_login (const char* username);

// SERVER
int tatl_init_server (int port, int flags);
int tatl_run_server ();

#endif
