/*
 * Copyright (c) 2007 Ryan Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#ifndef _EV_TCP_SOCKET_H_
#define _EV_TCP_SOCKET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <glib.h>
#include <ev.h>

#define EV_TCP_WARNING 0 /* just a warning, tell the user */
#define EV_TCP_ERROR   1 /* an error, the operation cannot complete */
#define EV_TCP_FATAL   2 /* an error, the operation must be aborted */
typedef void (*ev_tcp_error_cb) (int severity, char *message);

typedef struct ev_tcp_server ev_tcp_server;
typedef struct ev_tcp_client ev_tcp_client;

/* Callbacks */
typedef void (*ev_tcp_server_accept_cb) (ev_tcp_server *, ev_tcp_client *);

#define EV_TCP_COMMON           \
  int fd;                       \
  struct sockaddr_in sockaddr;  \
  int buf_size;                 \
  ev_tcp_error_cb error_cb;  

struct ev_tcp_server {
  EV_TCP_COMMON
  GQueue *children;
  
  ev_tcp_socket_accept_cb accept_cb;
  ev_io *accept_watcher;
  struct ev_loop *loop;
  
  char *write_buffer;
  size_t write_buffer_length;
};

ev_tcp_server* ev_tcp_server_new(ev_tcp_error_cb error_cb);
void ev_tcp_server_free(ev_tcp_server *socket);
void ev_tcp_server_close(ev_tcp_server *socket);
//char* ev_tcp_server_address(ev_tcp_server *socket);
void ev_tcp_server_listen( ev_tcp_server *socket
                         , char *address
                         , int port
                         , int backlog
                         , ev_tcp_socket_accept_cb
                         );


typedef void (*ev_tcp_client_read_cb)(ev_tcp_client*, char *buffer, size_t length);
struct ev_tcp_client {
  EV_TCP_COMMON
  
  ev_tcp_server *parent;
  
  ev_tcp_client_read_cb read_cb;
  ev_io *read_watcher;
  ev_io *write_watcher;
}
void ev_tcp_client_free(ev_tcp_client *client);
size_t ev_tcp_client_write(ev_tcp_client *, char *data, size_t length);


#endif _EV_TCP_SOCKET_H_