#ifndef EZ_TCP_H
#define EZ_TCP_H

/** 
 *  eztcp.h
 *  Written by Andi Cescolini and David Lopes
 *  Computer Networks, 2013
 *  University of Notre Dame
 *
 *  A library to get the ugly TCP stuff out of the way.
 *  It is not intended to be extensible or flexible -- it makes
 *  a lot of assumptions. But it's the assumptions we want.
 *
 */

// All functions return 0 on success and a value < 0 upon error. //

// Allow the user to control whether or not eztcp prints out errors
void ezsetprinterror(int p);

// Start a connection. New socket created is stored in sock
int ezconnect(int* sock, const char* ip, int port);

// Send data over the given socket
int ezsend(int sock, const void* data, int len);

// Receive data over the given socket
int ezreceive(int sock, void* data, int len);

// Listen for connections over a given port. New socket created is stored in sock
int ezlisten(int* sock, int port);

// Accept clients over a given socket
int ezaccept(int sock);

// Closes a connection
int ezclose(int sock);

// Gets socket data
void ezsocketdata(int sock, char* ip, int* port);

#endif
