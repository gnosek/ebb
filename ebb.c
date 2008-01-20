/* Ebb Web Server
 * Copyright (c) 2007 Ry Dahl
 * This software is released under the "MIT License". See README file for details.
 */

#include <glib.h>
#include <ev.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "mongrel/parser.h"
#include "tcp.h"

#include "ebb.h"
#include "parser_callbacks.h"

void* ebb_handle_request(void *_client);

ebb_server* ebb_server_new(struct ev_loop *loop)
{
  ebb_server *server = g_new0(ebb_server, 1);
  server->socket = tcp_listener_new(loop);
  return server;
}

void ebb_server_free(ebb_server *server)
{
  tcp_listener_free(server->socket);
}

void ebb_server_stop(ebb_server *server)
{
  tcp_listener_close(server->socket);
}

void ebb_on_read(char *buffer, int length, void *_client)
{
  ebb_client *client = (ebb_client*)(_client);
  assert(client);
  assert(client->socket);
  assert(client->socket->open);
  //assert(client->socket->open);
  assert(!http_parser_is_finished(&(client->parser)));
  
  g_string_append_len(client->buffer, buffer, length);
  
  http_parser_execute( &(client->parser)
                     , client->buffer->str
                     , client->buffer->len
                     , client->parser.nread
                     );
  
  if(http_parser_is_finished(&(client->parser))) {
    ebb_handle_request(client);
  }
}


/* User is responsible for freeing the client */
void* ebb_handle_request(void *_client)
{
  const char *ebb_input = "ebb.input";
  const char *server_name = "SERVER_NAME";
  const char *server_port = "SERVER_PORT";
  ebb_client *client = (ebb_client*)(_client);
  
  assert(client);
  assert(client->socket);
  assert(client->socket->open);
  
  g_queue_push_head(client->env, 
    ebb_env_pair_new(ebb_input, strlen(ebb_input), client->buffer->str, client->buffer->len));
  
  g_queue_push_head(client->env, ebb_env_pair_new2(server_name, 
    tcp_listener_address(client->server->socket)));
  
  g_queue_push_head(client->env, ebb_env_pair_new2(server_port, 
    client->server->socket->port_s));
  
  client->server->request_cb(client, client->server->request_cb_data);
  /* Cannot access client beyond this point because it's possible that the
   * user has freed it.
   */
  
  return NULL;
}


void ebb_on_request(tcp_peer *socket, void *data)
{
  ebb_server *server = (ebb_server*)(data);
  ebb_client *client = ebb_client_new(server, socket);
  
  assert(client->socket->open);
  assert(server->socket->open);
  
  socket->read_cb = ebb_on_read;
  socket->read_cb_data = client;
}

void ebb_server_start( ebb_server *server
                     , char *host
                     , int port
                     , ebb_request_cb_t request_cb
                     , void *request_cb_data
                     )
{
  server->request_cb = request_cb;
  server->request_cb_data = request_cb_data;
  tcp_listener_listen(server->socket, host, port, ebb_on_request, server);
}

ebb_client* ebb_client_new(ebb_server *server, tcp_peer *socket)
{
  ebb_client *client = g_new0(ebb_client, 1);
  
  client->server = server;
  client->socket = socket;
  
  /* http_parser */
  http_parser_init(&(client->parser));
  client->parser.data = client;
  client->parser.http_field = ebb_http_field_cb;
  client->parser.request_method = ebb_request_method_cb;
  client->parser.request_uri = ebb_request_uri_cb;
  client->parser.fragment = ebb_fragment_cb;
  client->parser.request_path = ebb_request_path_cb;
  client->parser.query_string = ebb_query_string_cb;
  client->parser.http_version = ebb_http_version_cb;
  client->parser.header_done = ebb_header_done_cb;
  
  /* buffer */
  client->buffer = g_string_new("");
  
  /* env */
  client->env = g_queue_new();
  
  return client;
}

void ebb_client_close(ebb_client *client)
{
  assert(client->socket->open);
  tcp_peer_close(client->socket);
}

void ebb_client_free(ebb_client *client)
{
  tcp_peer_close(client->socket);
  
  /* http_parser */
  http_parser_finish(&(client->parser));
  
  /* buffer */
  g_string_free(client->buffer, TRUE);
  
  /* env */
  ebb_env_pair *pair;
  while((pair = g_queue_pop_head(client->env)))
    ebb_env_pair_free(pair);
  g_queue_free(client->env);
  
  free(client);
}

// writes to the client
int ebb_client_write(ebb_client *this, const char *data, int length)
{
  return tcp_peer_write(this->socket, data, length);
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
