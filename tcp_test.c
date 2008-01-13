#include "tcp.h"
#include <stdio.h>
#include <stdlib.h>

/* Unit tests */
void unit_test_error( const gchar *log_domain
                    , GLogLevelFlags log_level
                    , const gchar *message
                    , gpointer user_data
                    )
{
  printf("ERROR(%d) %s\n", log_level, message);
  if(log_level < G_LOG_LEVEL_ERROR) { exit(1); }
}

GString *unit_test_input;

void unit_test_read_cb(char *buffer, int length, void *data)
{
  tcp_client *client = (tcp_client*)(data);
  char *reversed = g_utf8_strreverse(buffer, length);
  
  printf("read_cb called!\n");
  sleep(1);  
  //g_string_append_len(unit_test_input, buffer, length);
  
  tcp_client_write(client, reversed, length);
  
  free(reversed);
}

void unit_test_accept(tcp_client *client, void *data)
{
  fprintf(stdout, "Connection\n");
  client->read_cb = unit_test_read_cb;
  client->read_cb_data = client;
}

int main(void)
{
  tcp_server *server;
  unit_test_input = g_string_new(NULL);
  server = tcp_server_new(unit_test_error);
  
  g_log_set_handler (TCP_LOG_DOMAIN, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                      | G_LOG_FLAG_RECURSION, unit_test_error, NULL);
  
  fprintf(stdout, "Starting server at 0.0.0.0 31337\n");
  
  tcp_server_listen(server, "localhost", 31337, 1024, unit_test_accept, NULL);
  
  tcp_server_close(server);
  
  return 0; // success
}
