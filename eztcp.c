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

#include <sys/types.h>
#include <netdb.h>

#include "eztcp.h"

//#define DEBUG

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
  int s = 0;
  if ((s = sendto(sock, data, len, 0, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when sending data");
  }
  return s;
}

int ezreceive (int sock, void* data, int len) {
  int n, bytes_received = 0;
  socklen_t socklen = (sizeof(struct sockaddr_in));
  while (bytes_received < len) {
    if ((n = recvfrom(sock, data+bytes_received, len-bytes_received, 0, NULL, &socklen)) <= 0) {
      if (EZ_PRINT_ERROR && n<0) perror("Error when receiving data");
      if(EZ_PRINT_ERROR && n==0) perror("Closing socket");
      return n;
    } else {
      bytes_received += n;
    }    
  }
  return bytes_received;
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
  struct sockaddr_in cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  int conn;
#ifdef DEBUG
  printf("DEBUG: Attempting accept on socket: %d\n", sock);
#endif
  if ((conn = accept(sock, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
    if (EZ_PRINT_ERROR) perror("Error when accepting new client");
  }
  return conn;
}

int ezclose (int sock) {
  return close(sock);
}

void ezsocketdata(int sock, char* ip, int* port) {
  struct sockaddr_in sa;
  socklen_t sa_len = sizeof(sa);

  getsockname(sock, (struct sockaddr*)&sa, &sa_len);
  if (ip) strcpy(ip, inet_ntoa(sa.sin_addr));
  if (port) *port = (int)ntohs(sa.sin_port);

 


}

void ezpeerdata(int sock, char* ip, int* port) {
  struct sockaddr_in sa;
  socklen_t sa_len = sizeof(sa);

  getpeername(sock, (struct sockaddr*)(&sa), &sa_len);
  if (ip) strcpy(ip, inet_ntoa(sa.sin_addr));
  if (port) *port = (int)ntohs(sa.sin_port);
}

int ezconnect2 (int* sock, const char* ip, const char* port, char * server_ip) {
  int sockfd;  
  struct addrinfo hints, *servinfo, *p;
  int rv;

  char ipstr[INET_ADDRSTRLEN];
 

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
			 p->ai_protocol)) == -1) {
      perror("socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("connect");
      continue;
    }


    break;
  }

  if (p == NULL) {
    // looped off the end of the list with no connection
    fprintf(stderr, "failed to connect\n");
    exit(2);
  }

  //CODE TO GET THE IP ADDRESS FOR CLIENT TO STORE//
  struct in_addr *addr;
  struct sockaddr_in *ipv = (struct sockaddr_in *)p->ai_addr;
  addr = &(ipv->sin_addr);

  inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));

  strcpy(server_ip, ipstr);


  freeaddrinfo(servinfo); // all done with this structure


  *sock = sockfd;
  return 0;
}



