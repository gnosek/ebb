/*
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#ifndef tcp_server_h
#define tcp_server_h

#include <sys/socket.h>
#include <netinet/in.h>
#include <glib.h>
#include <ev.h>

#define TCP_WARNING 0 /* just a warning, tell the user */
#define TCP_ERROR   1 /* an error, the operation cannot complete */
#define TCP_FATAL   2 /* an error, the operation must be aborted */
typedef void (*tcp_error_cb) (int severity, char *message);

typedef struct tcp_client tcp_client;
typedef struct tcp_server tcp_server;

#define TCP_COMMON              \
  int fd;                       \
  struct sockaddr_in sockaddr;  \
  int buf_size;                 \
  tcp_error_cb error_cb;

/*** TCP Server ***/

typedef void (*tcp_server_accept_cb) (tcp_server *, tcp_client *);

tcp_server* tcp_server_new(tcp_error_cb error_cb);
void tcp_server_free(tcp_server*);
void tcp_server_close(tcp_server*);
void tcp_server_listen( tcp_server*
                      , char *address
                      , int port
                      , int backlog
                      , tcp_server_accept_cb
                      );

struct tcp_server {
  TCP_COMMON
  
  GQueue *children;
  
  tcp_server_accept_cb accept_cb;
  ev_io *accept_watcher;
  struct ev_loop *loop;
};

/*** TCP Client ***/

typedef void (*tcp_client_read_cb)( tcp_client*
                                  , char *buffer
                                  , int length
                                  );

void tcp_client_free(tcp_client*);
void tcp_client_close(tcp_client*);
int tcp_client_write(tcp_client *, const char *data, int length);
#define tcp_client_set_read_cb(client, _read_cb) client->read_cb=_read_cb

struct tcp_client {
  TCP_COMMON
  
  tcp_server *parent;
  
  tcp_client_read_cb read_cb;
  ev_io *read_watcher;
};

#endif tcp_server_h