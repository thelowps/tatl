/** 
 *  eztcp.c
 *  Written by David Lopes
 *  University of Notre Dame
 *
 *  A library to get the ugly TCP stuff out of the way.
 *  It is not intended to be extensible or flexible -- it makes
 *  a lot of assumptions. But it's the assumptions we want.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "eztcp.h"

// A switch to control whether or not the library prints out
// errors or just returns error values.
int EZ_PRINT_ERROR = 1; // OH GOD A GLOBAL VARIABLE RUN

void ezsetprinterror (int p) {
  EZ_PRINT_ERROR = p;
}

int ezconnect (int* sock, const char* ip, int port) {
  int s, err;
  struct sockaddr_in servaddr;
  
  // OPEN SOCKET
  if ((s = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when initializing socket descriptor");
    return s;
  }

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(ip);
  servaddr.sin_port = htons(port);

  // CONNECT
  if ((err = connect(s, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when connecting");
    return err;
  }

  *sock = s;
  return 0;
}

int ezsend (int sock, const void* data, int len) {
  struct sockaddr_in servaddr;
  int s;
  if ((s = sendto(sock, data, len, 0, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when sending data");
  }
  return s;
}

int ezreceive (int sock, void* data, int len) {
  int n;
  socklen_t socklen = (sizeof(struct sockaddr_in));
  if ((n = recvfrom(sock, data, len, 0, NULL, &socklen)) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when receiving data");
  } 
  return n;
}
	       
int ezlisten (int* sock, int port) {
  int listenfd, err;
  struct sockaddr_in servaddr;
  
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when initializing socket descriptor");
    return listenfd;
  }

  // This makes our server relinquish the port for reuse upon a crash
  int optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if ((err = bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr))) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when binding socket");
    return err;
  }
  if ((err = listen(listenfd, 1)) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when listening on socket");
    return err;
  }
  
  *sock = listenfd;
  return 0;
}

int ezaccept (int sock) {
  socklen_t clilen;
  struct sockaddr_in cliaddr;
  int conn;
  if ((conn = accept(sock, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when accepting new client");
  }
  return conn;
}

int ezclose (int sock) {
  return close(sock);
}
