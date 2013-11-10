#include "tatl.h"
#include <stdio.h>

int main (int argc, char* argv[]) {
  
  if (argc < 2) {
    printf("enter username.\n");
    return 1;
  }

  tatl_init_client (TATL_DEFAULT_SERVER_IP, TATL_DEFAULT_SERVER_PORT, 0);
  if (tatl_login(argv[1])) {
    printf("error logging in.\n");
    return 1;
  }

  printf("CLIENT: logged in!\n");
  tatl_create_room("three four seven");

  return 0;
}
