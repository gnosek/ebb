#include "tcp_server.h"
#include "error_callback.h"
#include <stdio.h>
#include <stdlib.h>

/* Unit tests */
void unit_test_error(int severity, char *message)
{
  printf("ERROR(%d) %s\n", severity, message);
  if(severity == ERROR_CB_FATAL) { exit(1); }
}

GString *unit_test_input;

void unit_test_read_cb(tcp_client *client, char *buffer, int length, void *data)
{
  GString *reversed = g_string_new(g_utf8_strreverse(buffer, length));
  
  //g_string_append_c(reversed, '\n');
  
  g_string_append_len(unit_test_input, buffer, length);
  
  tcp_client_write(client, reversed->str, reversed->len);
}

void unit_test_accept(tcp_server *server, tcp_client *client, void *data)
{
  fprintf(stdout, "Connection!\n");
  tcp_client_set_read_cb(client, unit_test_read_cb);
  
  //tcp_client_close(client);
  //tcp_server_close(server);
}

int main(void)
{
  tcp_server *server;
  unit_test_input = g_string_new(NULL);
  server = tcp_server_new(unit_test_error);
  
  fprintf(stdout, "Starting server at 0.0.0.0 31337\n");
  
  tcp_server_listen(server, "localhost", 31337, 1024, unit_test_accept, NULL);
  
  tcp_server_close(server);
  
  return 0; // success
}
