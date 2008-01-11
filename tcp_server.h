/* Evented TCP Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#ifndef tcp_server_h
#define tcp_server_h

#include <sys/socket.h>
#include <netinet/in.h>
#include <glib.h>
#include <ev.h>
#include "error_callback.h"

typedef struct tcp_client tcp_client;
typedef struct tcp_server tcp_server;

#define TCP_COMMON              \
  int fd;                       \
  struct sockaddr_in sockaddr;  \
  int buf_size;                 \
  error_cb_t error_cb;

/*** TCP Server ***/

typedef void (*tcp_server_accept_cb_t) (tcp_server *, tcp_client *, void *callback_data);

tcp_server* tcp_server_new(error_cb_t);
void tcp_server_free(tcp_server*);
void tcp_server_close(tcp_server*);
void tcp_server_listen( tcp_server*
                      , char *address
                      , int port
                      , int backlog
                      , tcp_server_accept_cb_t
                      , void *accept_cb_data
                      );

struct tcp_server {
  TCP_COMMON
  
  GQueue *children; /* TODO: implement me - fill with clients */
  
  void *accept_cb_data;
  tcp_server_accept_cb_t accept_cb;
  
  ev_io *accept_watcher;
  struct ev_loop *loop;
};

/*** TCP Client ***/

typedef void (*tcp_client_read_cb_t)(tcp_client*, char *buffer, int length, void *data);

void tcp_client_free(tcp_client*);
void tcp_client_close(tcp_client*);
int tcp_client_write(tcp_client *, const char *data, int length);
#define tcp_client_set_read_cb(client, _read_cb) client->read_cb=_read_cb;
#define tcp_client_set_read_cb_data(client, data) client->read_cb_data=data;

struct tcp_client {
  TCP_COMMON
  
  tcp_server *parent;
  
  void *read_cb_data;
  tcp_client_read_cb_t read_cb;
  
  ev_io *read_watcher;
};

#endif tcp_server_h