/* Ebb Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#include <glib.h>
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "mongrel/parser.h"
#include "tcp_server.h"
#include "ebb.h"

ebb_server* ebb_server_new()
{
  ebb_server *server = g_new0(ebb_server, 1);
  server->tcp_server = tcp_server_new();
  return server;
}

void ebb_server_free(ebb_server *server)
{
  tcp_server_free(server->tcp_server);
}

void ebb_server_stop(ebb_server *h)
{
  tcp_server_close(h->tcp_server);
}

void ebb_server_on_read(tcp_client *client, char *buffer, int length, void *data)
{
  http_parser *parser = (http_parser*)(data);
  
  http_parser_execute(parser, buffer, length, 0);
}

const char *ebb_input = "ebb.input";
const char *server_name = "SERVER_NAME";
const char *server_port = "SERVER_PORT";
void* ebb_handle_request(void *_request)
{
  ebb_request *request = (ebb_request*)(_request);
  
  g_queue_push_head(request->env, 
    ebb_env_pair_new(ebb_input, strlen(ebb_input), request->buffer->str, request->buffer->len));
  
  g_queue_push_head(request->env, ebb_env_pair_new2(server_name, 
    tcp_server_address(request->server->tcp_server)));
  
  g_queue_push_head(request->env, ebb_env_pair_new2(server_port, 
    request->server->tcp_server->port_s));
  
  request->server->request_cb(request, request->server->request_cb_data);
  
  pthread_exit(NULL);
  return NULL;
}

void ebb_on_read(char *buffer, int length, void *_request)
{
  ebb_request *request = (ebb_request*)(_request);
  
  // remove the read callback? so this isn't called again?  
  if(http_parser_is_finished(request->parser)) return;
  
  assert(request);
  
  g_string_append_len(request->buffer, buffer, length);
  
  http_parser_execute( request->parser
                     , request->buffer->str
                     , request->buffer->len
                     , request->parser->nread
                     );
  
  if(http_parser_is_finished(request->parser)) {
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, ebb_handle_request, request);
    if(rc < 0) {
      ebb_error("Could not create thread: %s", strerror(errno));
      return;
    }
  }
}

void ebb_on_request(tcp_server *server, tcp_client *client, void *data)
{
  ebb_server *h = (ebb_server*)(data);
  ebb_request *request = ebb_request_new(h, client);
  
  client->read_cb = ebb_on_read;
  client->read_cb_data = request;
}

void ebb_server_start(ebb_server *server
                      , char *host
                      , int port
                      , ebb_request_cb_t request_cb
                      , void *request_cb_data
                      )
{
  server->request_cb = request_cb;
  server->request_cb_data = request_cb_data;
  tcp_server_listen(server->tcp_server, host, port, 950, ebb_on_request, server);
}

#include "parser_callbacks.h"

ebb_request* ebb_request_new(ebb_server *server, tcp_client *client)
{
  ebb_request *request = g_new0(ebb_request, 1);
  
  request->server = server;
  request->client = client;
  
  /* http_parser */
  request->parser = g_new0(http_parser, 1);
  http_parser_init(request->parser);
  request->parser->data = request;
  request->parser->http_field = ebb_http_field_cb;
  request->parser->request_method = ebb_request_method_cb;
  request->parser->request_uri = ebb_request_uri_cb;
  request->parser->fragment = ebb_fragment_cb;
  request->parser->request_path = ebb_request_path_cb;
  request->parser->query_string = ebb_query_string_cb;
  request->parser->http_version = ebb_http_version_cb;
  request->parser->header_done = ebb_header_done_cb;
  
  /* buffer */
  request->buffer = g_string_new("");
  
  /* env */
  request->env = g_queue_new();
  
  return request;
}

void ebb_request_free(ebb_request *request)
{
  /* http_parser */
  http_parser_finish(request->parser);
  
  /* buffer */
  g_string_free(request->buffer, TRUE);
  
  /* env */
  ebb_env_pair *pair;
  while((pair = g_queue_pop_head(request->env)))
    ebb_env_pair_free(pair);
  g_queue_free(request->env);
  
  free(request);
}

ebb_env_pair* ebb_env_pair_new(const char *field, size_t flen, const char *value, size_t vlen)
{
  ebb_env_pair *pair = g_new(ebb_env_pair, 1);
  pair->field = field;
  pair->flen = flen;
  pair->value = value;
  pair->vlen = vlen;
  return pair;
}