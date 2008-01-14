/* Evented TCP Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */

#ifndef tcp_h
#define tcp_h

#include <sys/socket.h>
#include <netinet/in.h>
#include <glib.h>
#include <ev.h>

typedef struct tcp_client tcp_client;
typedef struct tcp_server tcp_server;


#define TCP_LOG_DOMAIN "TCP"
#define tcp_error(str, ...)  \
  g_log(TCP_LOG_DOMAIN, G_LOG_LEVEL_ERROR, str, ## __VA_ARGS__);

#define TCP_COMMON              \
  int fd;                       \
  struct sockaddr_in sockaddr;  \
  int open;

/*** TCP Server ***/
/* User is responsible for closing and freeing the tcp_client */
typedef void (*tcp_server_accept_cb_t) (tcp_client *, void *callback_data);

tcp_server* tcp_server_new();
void tcp_server_free(tcp_server*);
void tcp_server_close(tcp_server*);
void tcp_server_listen( tcp_server*
                      , char *address
                      , int port
                      , int backlog
                      , tcp_server_accept_cb_t
                      , void *accept_cb_data
                      );
char* tcp_server_address(tcp_server*);

struct tcp_server {
  TCP_COMMON
  struct hostent *dns_info;
  char *port_s;
  
  GQueue *clients;
  
  void *accept_cb_data;
  tcp_server_accept_cb_t accept_cb;
  
  ev_io *accept_watcher;
  struct ev_loop *loop;
};

/*** TCP Client ***/
typedef void (*tcp_client_read_cb_t)(char *buffer, int length, void *data);

void tcp_client_free(tcp_client *client);
void tcp_client_close(tcp_client*);
int tcp_client_write(tcp_client *, const char *data, int length);

struct tcp_client {
  TCP_COMMON
  
  tcp_server *parent;
  
  void *read_cb_data;
  tcp_client_read_cb_t read_cb;
  char *read_buffer;
  ev_io *read_watcher;
};

#endif tcp_h