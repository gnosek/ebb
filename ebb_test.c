#include "ebb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *header = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\n";

void request_cb(ebb_client *client, void *data)
{
  int i;
  //g_message("Request");
  
  ebb_client_write(client, header, strlen(header));
  
  for(i=0; i<client->env_size; i++) {
    ebb_client_write(client, client->env_fields[i], client->env_field_lengths[i]);
    ebb_client_write(client, "\r\n", 2);
    ebb_client_write(client, client->env_values[i], client->env_value_lengths[i]);
    ebb_client_write(client, "\r\n\r\n", 4);
  }
  
  ebb_client_write(client, "Hello.\r\n\r\n", 6);
  ebb_client_free(client);
}

int main(void)
{
  struct ev_loop *loop = ev_default_loop(0);
  ebb_server *server = ebb_server_new(loop);
  
  fprintf(stdout, "Starting server at 0.0.0.0 4001\n");
  
  ebb_server_start(server, "localhost", 4001, request_cb, NULL);
  ev_loop(loop, 0);
  ebb_server_free(server);
  return 0; // success
}
