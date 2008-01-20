/* Ebb Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */

#include <glib.h>
#include <ev.h>
#include "tcp.h"
#include "mongrel/parser.h"

#ifndef ebb_h
#define ebb_h

typedef struct ebb_server ebb_server;
typedef struct ebb_client ebb_client;

#define EBB_LOG_DOMAIN "Ebb"
#define ebb_error(str, ...)  \
  g_log(EBB_LOG_DOMAIN, G_LOG_LEVEL_ERROR, str, ## __VA_ARGS__);


/*** Ebb Client ***/

ebb_client* ebb_client_new(ebb_server *, tcp_peer *);
void ebb_client_free(ebb_client*);
void ebb_client_close(ebb_client*);
int ebb_client_write(ebb_client*, const char *data, int length);
#define ebb_client_add_env(client, field,flen,value,vlen) \
  client->env_fields[client->env_size] = field; \
  client->env_field_lengths[client->env_size] = flen; \
  client->env_values[client->env_size] = value; \
  client->env_value_lengths[client->env_size] = vlen; \
  client->env_size += 1;


#define MAX_ENV 100

struct ebb_client {
  ebb_server *server;
  tcp_peer *socket;
  http_parser parser;
  GString *buffer;
  
  /* the ENV structure */
  int env_size;
  const char *env_fields[MAX_ENV];
  size_t env_field_lengths[MAX_ENV];
  const char *env_values[MAX_ENV];
  size_t env_value_lengths[MAX_ENV];
};

/*** Ebb Server ***/

typedef void (*ebb_request_cb_t)(ebb_client*, void*);

ebb_server* ebb_server_new(struct ev_loop *loop);
void ebb_server_free(ebb_server*);
void ebb_server_stop(ebb_server*);
void ebb_server_start(ebb_server*, char *host, int port, ebb_request_cb_t, void *request_cb_data);

struct ebb_server {
  tcp_listener *socket;
  void *request_cb_data;
  ebb_request_cb_t request_cb;
  struct ebb_client clients[TCP_MAX_PEERS];
};

#endif ebb_h
