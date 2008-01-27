/* Ebb Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <glib.h>
#include <ev.h>
#include "mongrel/parser.h"

#ifndef ebb_h
#define ebb_h

typedef struct ebb_server ebb_server;
typedef struct ebb_client ebb_client;

#define EBB_LOG_DOMAIN "Ebb"
#define ebb_error(str, ...)  \
  g_log(EBB_LOG_DOMAIN, G_LOG_LEVEL_ERROR, str, ## __VA_ARGS__);
#define ebb_warning(str, ...)  \
  g_log(EBB_LOG_DOMAIN, G_LOG_LEVEL_WARNING, str, ## __VA_ARGS__);
#define ebb_info(str, ...)  \
  g_log(EBB_LOG_DOMAIN, G_LOG_LEVEL_INFO, str, ## __VA_ARGS__);

#define EBB_CHUNKSIZE (16*1024)
#define EBB_MAX_CLIENTS 950
#define EBB_TIMEOUT 30.0
#define MAX_ENV 100
#define EBB_TCP_COMMON          \
  unsigned open : 1;            \
  int fd;                       \
  struct sockaddr_in sockaddr;

/*** Ebb Client ***/

void ebb_client_close(ebb_client*);
ssize_t ebb_client_write(ebb_client*, const char *data, int length);
#define ebb_client_closed_p(client) (client->socket->open == FALSE)
#define ebb_client_add_env(client, field,flen,value,vlen) \
  client->env_fields[client->env_size] = field; \
  client->env_field_lengths[client->env_size] = flen; \
  client->env_values[client->env_size] = value; \
  client->env_value_lengths[client->env_size] = vlen; \
  client->env_size += 1;
#define ebb_client_add_env_const(client,field,value,vlen) \
  client->env_fields[client->env_size] = NULL; \
  client->env_field_lengths[client->env_size] = field; \
  client->env_values[client->env_size] = value; \
  client->env_value_lengths[client->env_size] = vlen; \
  client->env_size += 1;

enum { EBB_REQUEST_METHOD
     , EBB_REQUEST_URI
     , EBB_FRAGMENT
     , EBB_REQUEST_PATH
     , EBB_QUERY_STRING
     , EBB_HTTP_VERSION
     , EBB_REQUEST_BODY
     , EBB_SERVER_NAME
     , EBB_SERVER_PORT
     };

struct ebb_client {
  EBB_TCP_COMMON
  
  ebb_server *server;
  http_parser parser;
  
  char read_buffer[EBB_CHUNKSIZE];
  ssize_t read;
  ev_io read_watcher;
  
  ev_timer timeout_watcher;
  
  /* the ENV structure */
  int env_size;
  const char *env_fields[MAX_ENV];
  int env_field_lengths[MAX_ENV];
  const char *env_values[MAX_ENV];
  int env_value_lengths[MAX_ENV];
};

/*** Ebb Server ***/

typedef void (*ebb_request_cb)(ebb_client*, void*);

ebb_server* ebb_server_alloc();
void ebb_server_init( ebb_server *server
                    , struct ev_loop *loop
                    , char *address
                    , int port
                    , ebb_request_cb request_cb
                    , void *request_cb_data
                    );
void ebb_server_free(ebb_server*);
void ebb_server_start(ebb_server*);
void ebb_server_stop(ebb_server*);

struct ebb_server {
  EBB_TCP_COMMON
  struct hostent *dns_info;
  char *port;
  char *address;
  
  void *request_cb_data;
  ebb_request_cb request_cb;
  
  ev_io *request_watcher;
  struct ev_loop *loop;
  
  ebb_client clients[EBB_MAX_CLIENTS];
};


//const char *ebb_const_ebb_url_scheme = "ebb.url_scheme";
// DEF_GLOBAL(content_length, "CONTENT_LENGTH");
// DEF_GLOBAL(http_content_length, "HTTP_CONTENT_LENGTH");
// DEF_GLOBAL(content_type, "CONTENT_TYPE");
// DEF_GLOBAL(http_content_type, "HTTP_CONTENT_TYPE");
// DEF_GLOBAL(gateway_interface, "GATEWAY_INTERFACE");
// DEF_GLOBAL(gateway_interface_value, "CGI/1.2");
// DEF_GLOBAL(server_protocol, "SERVER_PROTOCOL");
// DEF_GLOBAL(server_protocol_value, "HTTP/1.1");
// DEF_GLOBAL(http_host, "HTTP_HOST");
// DEF_GLOBAL(port_80, "80");
// DEF_GLOBAL(url_scheme, "rack.url_scheme");
// DEF_GLOBAL(url_scheme_value, "http");
// DEF_GLOBAL(script_name, "SCRIPT_NAME");
// DEF_GLOBAL(path_info, "PATH_INFO");

#endif ebb_h
