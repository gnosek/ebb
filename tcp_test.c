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
  tcp_peer *peer = (tcp_peer*)(data);
  char *reversed = g_utf8_strreverse(buffer, length);
  
  printf("read_cb called!\n");
  //g_string_append_len(unit_test_input, buffer, length);
  
  tcp_peer_write(peer, reversed, length);
  
  free(reversed);
}

void unit_test_accept(tcp_peer *peer, void *data)
{
  fprintf(stdout, "Connection\n");
  peer->read_cb = unit_test_read_cb;
  peer->read_cb_data = peer;
}

int main(void)
{
  tcp_listener *listener;
  unit_test_input = g_string_new(NULL);
  listener = tcp_listener_new(unit_test_error);
  
  g_log_set_handler (TCP_LOG_DOMAIN, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                      | G_LOG_FLAG_RECURSION, unit_test_error, NULL);
  
  fprintf(stdout, "Starting listener at 0.0.0.0 1337\n");
  
  tcp_listener_listen(listener, "localhost", 1337, unit_test_accept, NULL);
  
  tcp_listener_free(listener);
  
  return 0; // success
}
