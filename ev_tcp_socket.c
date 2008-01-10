#include "ev_tcp_socket.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <assert.h>

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

ev_tcp_client* ev_tcp_client_new(ev_tcp_server *parent)
{
  socklen_t len;
  ev_tcp_client *client;
  
  client = g_new0(ev_tcp_client, 1);
  client->error_cb = parent->error_cb;
  client->parent = parent;
  client->fd = accept(parent->fd, (struct sockaddr*)&(client->sockaddr), &len);
  if(client->fd < 0) {
    client->error_cb(EV_TCP_ERROR, "Could not get client socket");
    ev_tcp_client_free(client);
    return NULL;
  }
  
  int r = fcntl(client->fd, F_SETFL, O_NONBLOCK);
  if(r < 0) {
    server->error_cb(EV_TCP_WARNING, "setting nonblock mode failed");
  }
  
  return client;
}

void ev_tcp_client_free(ev_tcp_client *client)
{
  close(client->fd);
  free(client);
}

ev_tcp_socket* ev_tcp_server_new(ev_tcp_error_cb error_cb)
{
  int r;
  ev_tcp_socket *server = g_new0(ev_tcp_server, 1);
  
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
  const int buf_size = 4096;
  struct ev_io watcher;
  
  server->sockaddr.sin_family = AF_INET;
  server->sockaddr.sin_port = htons(port);
  //inet_aton(address, server->sockaddr.sin_addr);
  misc_lookup_host(address, &(server->sockaddr.sin_addr));
  
  r = setsockopt(server->fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(int));
  if(r < 0) {
    server->error_cb(EV_TCP_ERROR, "setsockopt failed");
    return;
  }
  
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

/* Unit tests */
void unit_test_error(int severity, char *message)
{
  printf("ERROR(%d) %s\n", severity, message);
  if(severity == EV_TCP_FATAL) { exit(1); }
}

void unit_test_accept(ev_tcp_socket *parent, ev_tcp_socket *child)
{
  ev_tcp_socket_close(child);
  ev_tcp_socket_close(parent);
}

int main(void)
{
  ev_tcp_socket *socket;
  
  socket = ev_tcp_socket_new(unit_test_error);
  
  if(0 == fork()) {
    ev_tcp_socket_listen(socket, "localhost", 31337, 1024, unit_test_accept);
    ev_tcp_socket_close(socket);
  } else {
    sleep(1);
    printf("netstat: %s\n", 0 == system("netstat -an | grep LISTEN | grep 31337 > /dev/null") ? "ok" : "FAIL");
    printf("telnet: %s\n", 0 == system("telnet 0.0.0.0 31337 | grep 'Connected to localhost.'") ? "ok" : "FAIL"); 
  }
  
  return 0; // success
}

