#include "evtcp_server.h"


/* Unit tests */
void unit_test_error(int severity, char *message)
{
  printf("ERROR(%d) %s\n", severity, message);
  if(severity == EV_TCP_FATAL) { exit(1); }
}

GString *unit_test_input;

void unit_test_read_cb(ev_tcp_client *client, char *buffer, int length)
{
  GString *reversed = g_string_new(g_utf8_strreverse(buffer, length));
  
  //g_string_append_c(reversed, '\n');
  
  g_string_append_len(unit_test_input, buffer, length);
  
  ev_tcp_client_write(client, reversed->str, reversed->len);
}

void unit_test_accept(ev_tcp_server *server, ev_tcp_client *client)
{
  fprintf(stdout, "Connection!\n");
  client->read_cb = unit_test_read_cb;
  //ev_tcp_client_close(client);
  //ev_tcp_server_close(server);
}

int main(void)
{
  ev_tcp_server *server;
  unit_test_input = g_string_new(NULL);
  server = ev_tcp_server_new(unit_test_error);
  
  fprintf(stdout, "Starting server at 0.0.0.0 31337\n");
  
  ev_tcp_server_listen(server, "localhost", 31337, 1024, unit_test_accept);
  
  ev_tcp_server_close(server);
  
  return 0; // success
}
