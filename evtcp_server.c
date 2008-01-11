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

#include "evtcp_server.h"

#define EV_TCP_CHUNKSIZE (16*1024)


int misc_lookup_host(char *address, struct in_addr *addr)
{
  struct hostent *host;
  host = gethostbyname(address);
  if (host && host->h_addr_list[0]) {
    memmove(addr, host->h_addr_list[0], sizeof(struct in_addr));
    return 0;
  }
  return -1;
}

/* Returns the number of bytes remaining to write */
int ev_tcp_client_write(ev_tcp_client *client, const char *data, int length)
{
  int sent = send(client->fd, data, length, 0);
  if(sent < 0) {    
    client->error_cb(EV_TCP_ERROR, strerror(errno));
    ev_tcp_client_close(client);
    return 0;
  }
  return sent;
}

void ev_tcp_client_on_readable( struct ev_loop *loop
                              , struct ev_io *watcher
                              , int revents
                              )
{
  ev_tcp_client *client = (ev_tcp_client*)(watcher->data);
  char buffer[EV_TCP_CHUNKSIZE];
  int length;
    
  if(client->read_cb == NULL) return;
  
  length = recv(client->fd, buffer, EV_TCP_CHUNKSIZE, 0);
  
  if(length < 0) {
    client->error_cb(EV_TCP_ERROR, strerror(errno));
    ev_tcp_client_close(client);
    return;
  }
  
  /* User needs to copy the data out of the buffer or process it before
   * leaving this function.
   */
  client->read_cb(client, buffer, length);
}

ev_tcp_client* ev_tcp_client_new(ev_tcp_server *server)
{
  socklen_t len;
  ev_tcp_client *client;
  
  client = g_new0(ev_tcp_client, 1);
  client->error_cb = server->error_cb;
  client->parent = server;
  client->fd = accept(server->fd, (struct sockaddr*)&(client->sockaddr), &len);
  if(client->fd < 0) {
    client->error_cb(EV_TCP_ERROR, "Could not get client socket");
    ev_tcp_client_free(client);
    return NULL;
  }
  
  int r = fcntl(client->fd, F_SETFL, O_NONBLOCK);
  if(r < 0) {
    server->error_cb(EV_TCP_WARNING, "setting nonblock mode failed");
  }
  
  client->read_watcher = g_new0(struct ev_io, 1);
  client->read_watcher->data = client;
  ev_init (client->read_watcher, ev_tcp_client_on_readable);
  ev_io_set (client->read_watcher, client->fd, EV_READ);
  ev_io_start (server->loop, client->read_watcher);
  
  return client;
}

void ev_tcp_client_free(ev_tcp_client *client)
{
  ev_tcp_client_close(client);  
  free(client);
}

void ev_tcp_client_close(ev_tcp_client *client)
{
  if(client->read_watcher) {
    printf("killing read watcher\n");
    ev_io_stop(client->parent->loop, client->read_watcher);
    free(client->read_watcher);
    client->read_watcher = NULL;
  }
  close(client->fd);
}

ev_tcp_server* ev_tcp_server_new(ev_tcp_error_cb error_cb)
{
  int r;
  ev_tcp_server *server = g_new0(ev_tcp_server, 1);
  
  server->fd = socket(PF_INET, SOCK_STREAM, 0);
  server->error_cb = error_cb;
  r = fcntl(server->fd, F_SETFL, O_NONBLOCK);
  if(r < 0) {
    server->error_cb(EV_TCP_WARNING, "setting nonblock mode failed");
  }
  // set SO_REUSEADDR?
  
  server->loop = ev_default_loop(0);
  
  return server;
}

void ev_tcp_server_free(ev_tcp_server *server)
{
  ev_tcp_server_close(server);
  free(server);
}

void ev_tcp_server_close(ev_tcp_server *server)
{
  if(server->accept_watcher) {
    printf("killing accept watcher\n");
    ev_io_stop(server->loop, server->accept_watcher);
    free(server->accept_watcher);
    server->accept_watcher = NULL;
  }
  close(server->fd);
}

void ev_tcp_server_accept( struct ev_loop *loop
                         , struct ev_io *watcher
                         , int revents
                         )
{
  ev_tcp_server *server = (ev_tcp_server*)(watcher->data);
  ev_tcp_client *client;
  
  assert(server->loop == loop);
  
  client = ev_tcp_client_new(server);
  //g_queue_push_head(server->children, (gpointer)client);
  
  if(server->accept_cb != NULL)
    server->accept_cb(server, client);
  
  return;
}

void ev_tcp_server_listen ( ev_tcp_server *server
                          , char *address
                          , int port
                          , int backlog
                          , ev_tcp_server_accept_cb accept_cb
                          )
{
  int r;
  
  server->sockaddr.sin_family = AF_INET;
  server->sockaddr.sin_port = htons(port);
  //inet_aton(address, server->sockaddr.sin_addr);
  misc_lookup_host(address, &(server->sockaddr.sin_addr));
  
  /* Other socket options. This could probably be fine tuned.
   * SO_SNDBUF       set buffer size for output
   * SO_RCVBUF       set buffer size for input
   * SO_SNDLOWAT     set minimum count for output
   * SO_RCVLOWAT     set minimum count for input
   * SO_SNDTIMEO     set timeout value for output
   * SO_RCVTIMEO     set timeout value for input
   */
  r = bind(server->fd, (struct sockaddr*)&(server->sockaddr), sizeof(server->sockaddr));
  if(r < 0) {
    server->error_cb(EV_TCP_ERROR, "bind failed");
    close(server->fd);
    return;
  }

  r = listen(server->fd, backlog);
  if(r < 0) {
    server->error_cb(EV_TCP_ERROR, "listen failed");
    return;
  }
  
  server->accept_watcher = g_new0(struct ev_io, 1);
  server->accept_watcher->data = server;
  server->accept_cb = accept_cb;
  
  ev_init (server->accept_watcher, ev_tcp_server_accept);
  ev_io_set (server->accept_watcher, server->fd, EV_READ);
  ev_io_start (server->loop, server->accept_watcher);
  ev_loop (server->loop, 0);
  return;
}