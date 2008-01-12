/* Drum Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "drum.h"
#include "tcp_server.h"
#include "mongrel/parser.h"

drum_server* drum_server_new()
{
  drum_server *h = g_new0(drum_server, 1);
  h->tcp_server = tcp_server_new();
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

void drum_on_read(char *buffer, int length, void *data)
{
  drum_request *request = (drum_request*)(data);
  
  // remove the read callback? so this isn't called again?  
  //if(http_parser_is_finished(request->parser)) return;
  
  assert(request);
  
  g_string_append_len(request->buffer, buffer, length);
  
  http_parser_execute( request->parser
                     , request->buffer->str
                     , request->buffer->len
                     , request->parser->nread
                     );
  
  if(http_parser_is_finished(request->parser)) {
    g_hash_table_insert(request->env, g_string_new("drum.input"), request->buffer);
    
    request->server->request_cb(request, request->server->request_cb_data); 
  }
}

void drum_on_request(tcp_server *server, tcp_client *client, void *data)
{
  drum_server *h = (drum_server*)(data);
  drum_request *request = drum_request_new(h, client);
  
  client->read_cb = drum_on_read;
  client->read_cb_data = request;
}

void drum_server_start(drum_server *h
                      , char *host
                      , int port
                      , drum_request_cb_t request_cb
                      , void *request_cb_data
                      )
{
  h->request_cb = request_cb;
  h->request_cb_data = request_cb_data;
  tcp_server_listen(h->tcp_server, host, port, 950, drum_on_request, h);
}

void drum_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  drum_request *request = (drum_request*)(data);

  GString *field_string = g_string_new_len(field, flen);
  GString *value_string = g_string_new_len(value, vlen);
  printf("field: %s, value: %s\n", field_string->str, value_string->str);
  // GString *field_string, *value_string; 
  // 
  // 
  // 
  // g_hash_table_insert(request->env, field_string, value_string);
}

void drum_element_cb(void *data, const char *at, size_t length)
{
  drum_request *request = (drum_request*)(data);
  
  printf("element callback called.\n");
}

/* stupid that i have to do things like this.
 * i wish c would die. (i guess i'm not helping that.)
 */
void g_string_free_but_keep_buffer(gpointer data)
{
  g_string_free(data, FALSE);
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
  request->parser->request_method = drum_element_cb;
  request->parser->request_uri = drum_element_cb;
  request->parser->fragment = drum_element_cb;
  request->parser->request_path = drum_element_cb;
  request->parser->query_string = drum_element_cb;
  request->parser->http_version = drum_element_cb;
  request->parser->header_done = drum_element_cb;
  
  /* buffer */
  request->buffer = g_string_new("");
  
  /* env */
  request->env = g_hash_table_new_full( NULL
                                      , NULL
                                      , g_string_free_but_keep_buffer
                                      , g_string_free_but_keep_buffer
                                      );
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

