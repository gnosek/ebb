/* Drum Web Server
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
#include "drum.h"

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

void* drum_handle_request(void *_request)
{
  const char *drum_input = "drum.input";
  drum_request *request = (drum_request*)(_request);
  drum_env_pair *drum_input_pair = drum_env_pair_new( drum_input
                                                    , strlen(drum_input)
                                                    , request->buffer->str
                                                    , request->buffer->len
                                                    );
  
  g_queue_push_head(request->env, drum_input_pair);
  request->server->request_cb(request, request->server->request_cb_data);
  
  pthread_exit(NULL);
  return NULL;
}

void drum_on_read(char *buffer, int length, void *_request)
{
  drum_request *request = (drum_request*)(_request);
  
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
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, drum_handle_request, request);
    if(rc < 0) {
      drum_error("Could not create thread: %s", strerror(errno));
      return;
    }
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
  drum_env_pair *pair = drum_env_pair_new(field, flen, value, vlen);
  
  g_queue_push_head(request->env, pair);
}

void drum_element_cb(void *data, const char *at, size_t length)
{
  //drum_request *request = (drum_request*)(data);
  
  printf("element callback called.\n");
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
  request->env = g_queue_new();
  
  return request;
}

void drum_request_free(drum_request *request)
{
  /* http_parser */
  http_parser_finish(request->parser);
  
  /* buffer */
  g_string_free(request->buffer, TRUE);
  
  /* env */
  drum_env_pair *pair;
  while(pair = g_queue_pop_head(request->env))
    drum_env_pair_free(pair);
  g_queue_free(request->env);
  
  free(request);
}

drum_env_pair* drum_env_pair_new(const char *field, size_t flen, const char *value, size_t vlen)
{
  drum_env_pair *pair = g_new(drum_env_pair, 1);
  pair->field = field;
  pair->flen = flen;
  pair->value = value;
  pair->vlen = vlen;
  return pair;
}