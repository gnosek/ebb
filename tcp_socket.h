/*
 * Copyright (c) 2007 Ryan Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#ifndef _TCP_SOCKET_H_
#define _TCP_SOCKET_H_

#include <sys/socket.h>
#include <glib.h>

#define TCP_SOCKET_WARNING 0 /* just a warning, tell the user */
#define TCP_SOCKET_ERROR   1 /* an error, the operation cannot complete */
#define TCP_SOCKET_FATAL   2 /* an error, the operation must be aborted */
typedef void (*tcp_socket_error_cb) (int severity, char *message);

typedef struct tcp_socket {
  int fd;
  int buf_size;
  tcp_socket_error_cb error_cb;
  struct sockaddr_in *sockaddr;
} tcp_socket;

tcp_socket* tcp_socket_new(tcp_socket_error_cb error_cb);
void tcp_socket_free(tcp_socket *socket);
void tcp_socket_close(tcp_socket *socket);
char* tcp_socket_address(tcp_socket *socket);
void tcp_socket_listen(tcp_socket *socket, char *address, int port, int backlog);
tcp_socket* tcp_socket_accept(tcp_socket *socket);

#endif _TCP_SOCKET_H_