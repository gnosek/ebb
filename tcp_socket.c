#include "tcp_socket.h"

tcp_socket* tcp_socket_new(tcp_socket_error_cb error_cb)
{
  tcp_socket *socket = g_new0(tcp_socket, 1);
  
  socket->fd = socket(PF_INET, SOCK_STREAM, 0);
  socket->error_cb = error_cb;
  fcntl(socket->fd, F_SETFL, O_NONBLOCK);
  // set SO_REUSEADDR?
  return socket;
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
  int addrlen;
  
  if(socket->sockaddr == NULL) return NULL;
  getpeername(socket->fd, (struct sockaddr*)(socket->sockaddr), &addrlen);
  return inet_ntoa(socket->sockaddr.sin_addr);
}

void tcp_socket_listen (tcp_socket *socket, char *address, int port, int backlog)
{
  const int buf_size = 4096;
  
  socket->sockaddr = g_new0(struct sockaddr_in, 1);
  socket->sockaddr.sin_family = AF_INET;
  socket->sockaddr.sin_port = htons(port);
  socket->sockaddr.sin_addr.s_addr = address; // FIXME
  
  setsockopt(socket->fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(int));
  
  bind(socket->fd, (struct sockaddr*)(socket->sockaddr), sizeof(struct sockaddr_in));
  listen(socket->fd, backlog);
}

tcp_socket* tcp_socket_accept(tcp_socket *socket)
{
  tcp_socket *client_socket = g_new0(tcp_socket, 1);
  socklen_t len;
  
  client_socket->error_cb = error_cb;
  client_socket->fd = accept(socket->fd, (struct sockaddr*)(client_socket->sockaddr), &len);
  // if(client_socket->fd < 0) { error }
  fcntl(client_socket->fd, F_SETFL, O_NONBLOCK);
  
  return client_socket;
}

/* Unit tests */
#include <stdio.h>
void main(void)
{
  tcp_socket *socket;
  
  socket = tcp_socket_new(0);
  printf("hello world");
}

