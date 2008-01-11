/* Drum Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#include <glib.h>
#include <stdlib.h>
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

void drum_on_read(tcp_client *client, char *buffer, int length, void *data)
{
  drum_request *request = (drum_request*)(data);

  // remove the read callback? so this isn't called again?  
  //if(http_parser_is_finished(request->parser)) return;
  
  g_string_append_len(request->buffer, buffer, length);
  
  http_parser_execute( request->parser
                     , request->buffer->str
                     , request->buffer->len
                     , request->parser->nread
                     );
  
  if(http_parser_is_finished(request->parser)) {
    g_hash_table_insert(request->env, g_string_new("drum.input"), request->buffer);
    
    request->server->request_cb(request, NULL); 
  }
}

void drum_on_request(tcp_server *server, tcp_client *client, void *data)
{
  drum_server *h = (drum_server*)(data);
  drum_request *request = drum_request_new(h, client);
  
  tcp_client_set_read_cb(client, drum_on_read);
  tcp_client_set_read_cb_data(client, request);
}

void drum_server_start(drum_server *h, char *host, int port, drum_request_cb_t request_cb)
{
  h->request_cb = request_cb;
  tcp_server_listen(h->tcp_server, host, port, 950, drum_on_request, h);
}

void drum_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  drum_request *request = (drum_request*)(data);
  GString *field_string, *value_string; 
  
  field_string = g_string_new_len(field, flen);
  value_string = g_string_new_len(value, vlen);
  g_hash_table_insert(request->env, field_string, value_string);
}

drum_request* drum_request_new(drum_server *server, tcp_client *client)
{
  drum_request *request = g_new0(drum_request, 1);
  
  request->server = server;
  request->client = client;
  
  /* http_parser */
  request->parser = g_new0(http_parser, 1);
  http_parser_init(request->parser);
  request->parser->data = request;
  request->parser->http_field = drum_http_field;
  
  /* buffer */
  request->buffer = g_string_new("");
  
  /* env */
  request->env = g_hash_table_new_full(NULL, NULL, g_string_free, g_string_free);
  
  return request;
}

void drum_request_free(drum_request *request)
{
  /* http_parser */
  http_parser_finish(request->parser);
  
  /* buffer */
  g_string_free(request->buffer, TRUE);
  
  /* env */
  g_hash_table_destroy(request->env);
  
  free(request);
}

