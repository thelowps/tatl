#include "tatl.h"

int main (void) {
  
  tatl_init_server(TATL_DEFAULT_SERVER_PORT, 0);
  tatl_run_server();
  
  return 0;
}
