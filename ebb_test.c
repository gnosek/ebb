#include "ebb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *header = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\n";

void request_cb(ebb_client *client, void *data)
{
  ebb_env_pair *pair;
  //g_message("Request");
  tcp_client_write(client->socket, header, strlen(header));
  
  while((pair = g_queue_pop_head(client->env))) {
    tcp_client_write(client->socket, pair->field, pair->flen);
    tcp_client_write(client->socket, "\r\n", 2);
    tcp_client_write(client->socket, pair->value, pair->vlen);
    tcp_client_write(client->socket, "\r\n\r\n", 4);
    
    ebb_env_pair_free(pair);
  }
  tcp_client_write(client->socket, "Hello.\r\n\r\n", 6);
  tcp_client_close(client->socket);
}

int main(void)
{
  ebb_server *server;
  server = ebb_server_new();
  
  fprintf(stdout, "Starting server at 0.0.0.0 31337\n");
  
  ebb_server_start(server, "localhost", 31337, request_cb, NULL);
  
  
  return 0; // success
}
