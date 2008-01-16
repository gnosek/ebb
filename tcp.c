/* Evented TCP Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */

/* TODO: add timeouts for clients */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <ev.h>
#include <glib.h>

#include <assert.h>

#include "tcp.h"

#define TCP_CHUNKSIZE (16*1024)

/* Private function */
void tcp_client_stop_read_watcher(tcp_client *client);

/* Returns the number of bytes remaining to write */
int tcp_client_write(tcp_client *client, const char *data, int length)
{
  if(!client->open) {
    tcp_warning("Trying to write to a client that isn't open.");
    return 0;
  }
  assert(client->open);
  int sent = send(client->fd, data, length, 0);
  if(sent < 0) {
    tcp_warning("Error writing: %s", strerror(errno));
    tcp_client_close(client);
    return 0;
  }
  ev_timer_again(client->parent->loop, client->timeout_watcher);
  
  return sent;
}

void tcp_client_on_timeout( struct ev_loop *loop
                          , struct ev_timer *watcher
                          , int revents
                          )
{
  tcp_client *client = (tcp_client*)(watcher->data);
  
  assert(client->parent->loop == loop);
  assert(client->timeout_watcher == watcher);
  
  tcp_client_close(client);
  tcp_info("client timed out");
}

void tcp_client_on_readable( struct ev_loop *loop
                           , struct ev_io *watcher
                           , int revents
                           )
{
  tcp_client *client = (tcp_client*)(watcher->data);
  int length;
  
  // check for error in revents
  if(EV_ERROR & revents) {
    tcp_error("tcp_client_on_readable() got error event, closing client");
    goto error;
  }
  
  assert(client->open);
  assert(client->parent->open);
  assert(client->parent->loop == loop);
  assert(client->read_watcher == watcher);
  
  if(client->read_cb == NULL) return;
  
  length = recv(client->fd, client->read_buffer, TCP_CHUNKSIZE, 0);
  
  if(length == 0) {
    g_debug("zero length read? what to do? killing read watcher");
    tcp_client_stop_read_watcher(client);
    return;
  } else if(length < 0) {
    if(errno == EBADF || errno == ECONNRESET)
      g_debug("errno says Connection reset by peer"); 
    else
      tcp_error("Error recving data: %s", strerror(errno));
    goto error;
  }
  
  ev_timer_again(loop, client->timeout_watcher);
  // g_debug("Read %d bytes", length);
  
  client->read_cb(client->read_buffer, length, client->read_cb_data);
  /* Cannot access client beyond this point because it's possible that the
   * user has freed it.
   */
   return;
error:
  tcp_client_close(client);
}

tcp_client* tcp_client_new(tcp_server *server)
{
  socklen_t len;
  tcp_client *client;
  
  client = g_new0(tcp_client, 1);
  
  client->parent = server;
  
  client->fd = accept(server->fd, (struct sockaddr*)&(client->sockaddr), &len);
  if(client->fd < 0) {
    tcp_error("Could not get client socket");
    goto error;
  }
  
  client->open = TRUE;
  
  int r = fcntl(client->fd, F_SETFL, O_NONBLOCK);
  if(r < 0) {
    tcp_error("Setting nonblock mode on socket failed");
    goto error;
  }
  
  client->read_buffer = (char*)malloc(sizeof(char)*TCP_CHUNKSIZE);
  
  client->read_watcher = g_new0(struct ev_io, 1);
  client->read_watcher->data = client;
  ev_init(client->read_watcher, tcp_client_on_readable);
  ev_io_set(client->read_watcher, client->fd, EV_READ | EV_ERROR);
  ev_io_start(server->loop, client->read_watcher);
  
  client->timeout_watcher = g_new0(struct ev_timer, 1);
  client->timeout_watcher->data = client;
  ev_timer_init(client->timeout_watcher, tcp_client_on_timeout, 60., 60.);
  ev_timer_start(server->loop, client->timeout_watcher);
  
  return client;
  
error:
  tcp_client_close(client);
  return NULL;
}

void tcp_client_stop_read_watcher(tcp_client *client)
{
  //assert(client->open);
  assert(client->parent->open);
  //assert(client->read_watcher);
  
  if(client->read_watcher != NULL) {
    //g_debug("killing read watcher");
    ev_io_stop(client->parent->loop, client->read_watcher);
    free(client->read_watcher);
    client->read_watcher = NULL;
  }
}

void tcp_client_free(tcp_client *client)
{
  tcp_client_close(client);
  free(client->read_buffer);
  free(client);
  //g_debug("tcp client closed");
}

void tcp_client_close(tcp_client *client)
{
  if(client->open) {
    tcp_client_stop_read_watcher(client);
    
    ev_timer_stop(client->parent->loop, client->timeout_watcher);
    free(client->timeout_watcher);
    client->timeout_watcher = NULL;
    
    close(client->fd);
    client->open = FALSE;
    //g_debug("tcp client closed");
  }
}

tcp_server* tcp_server_new()
{
  int r;
  
  tcp_server *server = g_new0(tcp_server, 1);
  
  server->fd = socket(PF_INET, SOCK_STREAM, 0);
  r = fcntl(server->fd, F_SETFL, O_NONBLOCK);
  if(r < 0) {
    tcp_error("Setting nonblock mode on socket failed");
    goto error;
  }
  
  int flags = 1;
  r = setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
  if(r < 0) {
    tcp_error("failed to set setsock to reuseaddr");
    goto error;
  }
  /*
  r = setsockopt(server->fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));
  if(r < 0) {
    tcp_error("failed to set socket to nodelay");
    tcp_server_free(server);
    return NULL;
  }
  */
  
  server->loop = ev_loop_new(0);
  
  server->clients = g_queue_new();
  server->open = FALSE;
  return server;

error:
  tcp_server_free(server);
  return NULL;
}

void tcp_server_free(tcp_server *server)
{
  tcp_server_close(server);
  g_queue_free(server->clients);
  free(server);
  
  g_debug("tcp server freed.");
}

void tcp_server_close(tcp_server *server)
{
  printf("closeserver\n");
  assert(server->open);
  
  tcp_client *client;
  while((client = g_queue_pop_head(server->clients)))
    tcp_client_close(client);
  
  if(server->port_s) {
    free(server->port_s);
    server->port_s = NULL;
  }
  if(server->dns_info) {
    free(server->dns_info);
    server->dns_info = NULL;
  }
  if(server->accept_watcher) {
    printf("killing accept watcher\n");
    ev_io_stop(server->loop, server->accept_watcher);
    free(server->accept_watcher);
    server->accept_watcher = NULL;
  }
  ev_unloop(server->loop, EVUNLOOP_ALL);
  ev_loop_destroy (server->loop);
  server->loop = NULL;
  
  close(server->fd);
  server->open = FALSE;
}

void tcp_server_accept( struct ev_loop *loop
                      , struct ev_io *watcher
                      , int revents
                      )
{
  tcp_server *server = (tcp_server*)(watcher->data);
  tcp_client *client;
  
  assert(server->open);
  assert(server->loop == loop);
  
  // check for error in revents
  if(EV_ERROR & revents) {
    tcp_error("tcp_client_on_readable() got error event, closing free");
    tcp_server_free(server);
    return;
  }
  
  client = tcp_client_new(server);
  g_queue_push_head(server->clients, (gpointer)client);
  
  if(server->accept_cb != NULL)
    server->accept_cb(client, server->accept_cb_data);
  
  return;
}

void tcp_server_listen ( tcp_server *server
                       , char *address
                       , int port
                       , int backlog
                       , tcp_server_accept_cb_t accept_cb
                       , void *accept_cb_data
                       )
{
  int r;
  
  server->sockaddr.sin_family = AF_INET;
  server->sockaddr.sin_port = htons(port);
  
  /* for easy access to the port */
  server->port_s = malloc(sizeof(char)*8);
  sprintf(server->port_s, "%d", port);
  
  server->dns_info = gethostbyname(address);
  if (!(server->dns_info && server->dns_info->h_addr)) {
    tcp_error("Could not look up hostname %s", address);
    goto error;
  }
  memmove(&(server->sockaddr.sin_addr), server->dns_info->h_addr, sizeof(struct in_addr));
  
  /* Other socket options. These could probably be fine tuned.
   * SO_SNDBUF       set buffer size for output
   * SO_RCVBUF       set buffer size for input
   * SO_SNDLOWAT     set minimum count for output
   * SO_RCVLOWAT     set minimum count for input
   * SO_SNDTIMEO     set timeout value for output
   * SO_RCVTIMEO     set timeout value for input
   */
  r = bind(server->fd, (struct sockaddr*)&(server->sockaddr), sizeof(server->sockaddr));
  if(r < 0) {
    tcp_error("Failed to bind to %s %d", address, port);
    goto error;
  }

  r = listen(server->fd, backlog);
  if(r < 0) {
    tcp_error("listen() failed");
    goto error;
  }
  
  assert(server->open == FALSE);
  server->open = TRUE;
  
  server->accept_watcher = g_new0(struct ev_io, 1);
  server->accept_watcher->data = server;
  server->accept_cb = accept_cb;
  server->accept_cb_data = accept_cb_data;
  
  ev_init (server->accept_watcher, tcp_server_accept);
  ev_io_set (server->accept_watcher, server->fd, EV_READ | EV_ERROR);
  ev_io_start (server->loop, server->accept_watcher);
  ev_loop (server->loop, 0);
  return;
  
error:
  tcp_server_close(server);
  return;
}

char* tcp_server_address(tcp_server *server)
{
  if(server->dns_info)
    return server->dns_info->h_name;
  else
    return NULL;
}
