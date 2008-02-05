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
#define ebb_debug(str, ...)  \
  g_log(EBB_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, str, ## __VA_ARGS__);

#define EBB_CHUNKSIZE (16*1024)
#define EBB_MAX_CLIENTS 200
#define EBB_TIMEOUT 30.0
#define EBB_MAX_ENV 100
#define EBB_TCP_COMMON          \
  unsigned open : 1;            \
  int fd;                       \
  struct sockaddr_in sockaddr;

/*** Ebb Client ***/

typedef void (*ebb_client_cb)(ebb_client*);

void ebb_client_close(ebb_client*);
void ebb_client_set_nonblocking(ebb_client *client);
void ebb_client_set_blocking(ebb_client *client);
void ebb_client_write(ebb_client*, const char *data, int length);
void ebb_client_start_writing( ebb_client *client
                             , ebb_client_cb after_write_cb
                             );
/* User must free the GString returned from ebb_client_read_input */
GString* ebb_client_read_input(ebb_client *client, ssize_t size);


enum { EBB_REQUEST_METHOD
     , EBB_REQUEST_URI
     , EBB_FRAGMENT
     , EBB_REQUEST_PATH
     , EBB_QUERY_STRING
     , EBB_HTTP_VERSION
     , EBB_REQUEST_BODY
     , EBB_SERVER_NAME
     , EBB_SERVER_PORT
     , EBB_CONTENT_LENGTH
     };

struct ebb_client {
  EBB_TCP_COMMON
  
  ebb_server *server;
  http_parser parser;
  
  char read_buffer[EBB_CHUNKSIZE];
  ssize_t read;
  ev_io read_watcher;
  
  ev_io write_watcher;
  GString *write_buffer;
  ssize_t written;
  ebb_client_cb after_write_cb;
  
  void *data;
  
  char *input_head;
  ssize_t input_head_len;
  ssize_t input_read;
  int content_length;
  
  ev_timer timeout_watcher;
  
  /* the ENV structure */
  int env_size;
  const char *env_fields[EBB_MAX_ENV];
  int  env_field_lengths[EBB_MAX_ENV];
  const char *env_values[EBB_MAX_ENV];
  int  env_value_lengths[EBB_MAX_ENV];
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

#endif ebb_h
