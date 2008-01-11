/* Drum Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#include <glib.h>
#include "drum.h"
#include "tcp_server.h"
#include "mongrel/parser.h"
#include "error_callback.h"

drum_server* drum_server_new(error_cb_t error_cb)
{
  drum_server *h = g_new0(drum_server, 1);
  h->tcp_server = tcp_server_new(error_cb);
  return h;
}

void drum_server_free(drum_server *h)
{
  tcp_server_free(h->tcp_server);
}

void drum_server_stop(drum_server *h)
{
  tcp_server_close(h->tcp_server);
}

void drum_server_on_read(tcp_client *client, char *buffer, int length, void *data)
{
  http_parser *parser = (http_parser*)(data);
  
  http_parser_execute(parser, buffer, length, 0);
}

void drum_server_on_request(tcp_server *server, tcp_client *client, void *data)
{
  drum_server *h = (drum_server*)(data);
  
  GHashTable *env = g_hash_table_new(NULL, NULL);
  
  h->request_cb(h, client, env);
}

void drum_server_start(drum_server *h, char *host, int port, drum_request_cb_t request_cb)
{
  h->request_cb = request_cb;
  tcp_server_listen(h->tcp_server, host, port, 950, drum_server_on_request, h);
}