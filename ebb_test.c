#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "ebb.h"


void request_cb(ebb_client *client, void *data)
{
  const char *header = "HTTP/1.1 200 OK\r\n"
                       "Connection: close\r\n"
                       "Content-Type: text/plain\r\n\r\n";
  int i;
  //g_message("Request");
  
  ebb_client_write(client, header, strlen(header));
  
  for(i=0; i<client->env_size; i++) {
    if(client->env_fields[i]) {
      ebb_client_write(client, client->env_fields[i], client->env_field_lengths[i]);
      ebb_client_write(client, "\r\n", 2);
      ebb_client_write(client, client->env_values[i], client->env_value_lengths[i]);
      ebb_client_write(client, "\r\n\r\n", 4);
    }
  }
  
  ebb_client_write(client, "Hello.\r\n\r\n", 6);
  ebb_client_start_writing(client, NULL);
}

int main(void)
{
  struct ev_loop *loop = ev_default_loop(0);
  ebb_server *server = ebb_server_alloc();
  
  ebb_server_init(server, loop, "localhost", 4001, request_cb, NULL);
  
  /* Ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);
  
  fprintf(stdout, "Starting server at 0.0.0.0 4001\n");
  
  ebb_server_start(server);
  ev_loop(loop, 0);
  ebb_server_free(server);
  return 0; // success
}
