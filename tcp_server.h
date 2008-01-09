/*
 * Copyright (c) 2007 Ryan Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include "tcp_server.h"
#include <glib.h>
#include <ev.h>

typedef struct evtcp_socket evtcp_socket;

/*** TCP Server ***/
typedef void (*evtcp_connection_cb) (evtcp_server *parent, evtcp_client *new_client);
typedef struct evtcp_server {
  int max_clients;
  int client_timeout;
  
  /* private */
  /* called for each client that connects */
  evtcp_connection_cb connection_cb;
  evtcp_socket *socket;
  struct ev_loop *loop;
  struct ev_io *watcher;  
  evtcp_error_cb error_cb;
  GQueue *clients;
} evtcp_server;

evtcp_server* evtcp_server_new(tcp_error_cb);
void evtcp_server_free(evtcp_server*);
void evtcp_server_bind(evtcp_server*, char *address, int port, evtcp_connection_cb);

/*** TCP Server Client ***/
typedef void (*evtcp_read_cb)(evtcp_client*, char *buffer, size_t length);
typedef struct evtcp_client {
  evtcp_server *parent; /* ro */
  /* called each time there is data to read */
  evtcp_read_cb read_cb; /* rw */
  
  /* private */
  bool closed;
  evtcp_socket *socket;
  struct ev_io *io_event;
  evtcp_error_cb error_cb;
  char *write_buffer;
  size_t write_buffer_length;
} evtcp_client;
/* returns the total number of bytes buffered after this write */
size_t evtcp_client_write(evtcp_client_t *, char *data, size_t length);
void evtcp_client_close(evtcp_client*);

#endif _TCP_SERVER_H_