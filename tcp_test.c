#include "tcp.h"
#include <stdio.h>
#include <stdlib.h>

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
  struct ev_loop *loop = ev_default_loop (0);
  tcp_listener *listener;
  unit_test_input = g_string_new(NULL);
  
  listener = tcp_listener_new(loop);
  
  fprintf(stdout, "Starting listener at 0.0.0.0 4001\n");
  
  tcp_listener_listen(listener, "localhost", 4001, unit_test_accept, NULL);
  
  ev_loop(loop, 0);
  
  tcp_listener_free(listener);
  
  return 0; // success
}
