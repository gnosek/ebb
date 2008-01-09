#include "tcp_socket.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <assert.h>

/* private functions */
tcp_socket* tcp_socket_accept(tcp_socket *socket);

tcp_socket* tcp_socket_new(tcp_socket_error_cb error_cb)
{
  tcp_socket *s = g_new0(tcp_socket, 1);
  
  s->fd = socket(PF_INET, SOCK_STREAM, 0);
  s->error_cb = error_cb;
  fcntl(s->fd, F_SETFL, O_NONBLOCK);
  // set SO_REUSEADDR?
  return s;
}

void tcp_socket_free(tcp_socket *socket)
{
  tcp_socket_close(socket);
  free(socket);
}

void tcp_socket_close(tcp_socket *socket)
{
  close(socket->fd);
}

char* tcp_socket_address(tcp_socket *socket)
{
  unsigned int addrlen;
  
  getpeername(socket->fd, (struct sockaddr*)&(socket->sockaddr), &addrlen);
  return inet_ntoa(socket->sockaddr.sin_addr);
}

void tcp_socket_listen (tcp_socket *socket, char *address, int port, int backlog)
{
  const int buf_size = 4096;
  
  socket->sockaddr.sin_family = AF_INET;
  socket->sockaddr.sin_port = htons(port);
  inet_aton(address, &(socket->sockaddr.sin_addr));
  
  setsockopt(socket->fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(int));
  
  bind(socket->fd, (struct sockaddr*)&(socket->sockaddr), sizeof(socket->sockaddr));
  listen(socket->fd, backlog);
}

tcp_socket* tcp_socket_accept(tcp_socket *socket)
{
  tcp_socket *client_socket = g_new0(tcp_socket, 1);
  socklen_t len;
  
  client_socket->error_cb = socket->error_cb;
  client_socket->fd = accept(socket->fd, (struct sockaddr*)&(client_socket->sockaddr), &len);
  // if(client_socket->fd < 0) { error }
  fcntl(client_socket->fd, F_SETFL, O_NONBLOCK);
  
  return client_socket;
}

/* Unit tests */
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
  tcp_socket *socket;
  
  socket = tcp_socket_new(0);
  tcp_socket_free(socket);
  
  socket = tcp_socket_new(0);
  tcp_socket_listen(socket, "0.0.0.0", 3001, 1024);
  system("telnet localhost 3001");
  
  tcp_socket_free(socket);
  
  
  printf("hello world\n");
  return 0; // success
}

