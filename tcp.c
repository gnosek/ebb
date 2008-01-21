/* Evented TCP listener
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <ev.h>
#include <glib.h>
#include <assert.h>

#include "tcp.h"

/* Private function */
void tcp_peer_stop_read_watcher(tcp_peer *peer);

/* Returns the number of bytes remaining to write */
int tcp_peer_write(tcp_peer *peer, const char *data, int length)
{
  if(!peer->open) {
    tcp_warning("Trying to write to a peer that isn't open.");
    return 0;
  }
  
  int sent = send(peer->fd, data, length, MSG_HAVEMORE);
  if(sent < 0) {
    //tcp_warning("Error writing: %s", strerror(errno));
    tcp_peer_close(peer);
    return 0;
  }
  ev_timer_again(peer->parent->loop, &(peer->timeout_watcher));
  
  return sent;
}

void tcp_peer_on_timeout( struct ev_loop *loop
                        , struct ev_timer *watcher
                        , int revents
                        )
{
  tcp_peer *peer = (tcp_peer*)(watcher->data);
  
  assert(peer->parent->loop == loop);
  assert(&(peer->timeout_watcher) == watcher);
  
  tcp_peer_close(peer);
  tcp_info("peer timed out");
}

void tcp_peer_on_readable( struct ev_loop *loop
                           , struct ev_io *watcher
                           , int revents
                           )
{
  tcp_peer *peer = (tcp_peer*)(watcher->data);
  int length;
  
  // check for error in revents
  if(EV_ERROR & revents) {
    tcp_error("tcp_peer_on_readable() got error event, closing peer");
    goto error;
  }
  
  assert(peer->open);
  assert(peer->parent->open);
  assert(peer->parent->loop == loop);
  assert(&(peer->read_watcher) == watcher);
  
  if(peer->read_cb == NULL) return;
  
  length = recv(peer->fd, peer->read_buffer, TCP_CHUNKSIZE, 0);
  
  if(length == 0) {
    //g_debug("zero length read? what to do?");
    tcp_peer_close(peer);
    //tcp_peer_stop_read_watcher(peer);
    return;
  } else if(length < 0) {
    if(errno == EBADF || errno == ECONNRESET)
      g_debug("errno says Connection reset by peer"); 
    else
      tcp_error("Error recving data: %s", strerror(errno));
    goto error;
  }
  
  ev_timer_again(loop, &(peer->timeout_watcher));
  // g_debug("Read %d bytes", length);
  
  peer->read_cb(peer->read_buffer, length, peer->read_cb_data);
  /* Cannot access peer beyond this point because it's possible that the
   * user has freed it.
   */
   return;
error:
  tcp_peer_close(peer);
}

tcp_peer* tcp_peer_new(tcp_listener *listener)
{
  socklen_t len;
  tcp_peer *peer = NULL;
  int i;
  
  /* Get next availible peer */
  for(i=0; i < TCP_MAX_PEERS; i++)
    if(!listener->peers[i].open) {
      peer = &(listener->peers[i]);
      break;
    }
  
  if(peer == NULL) {
    g_message("Too many peers. Refusing connections.");
    return NULL;
  }
  
  peer->open = TRUE; /* set open ASAP */
  
  /* DEBUG */
  // int count = 0;
  // for(i = 0; i < TCP_MAX_PEERS; i++)
  //   if(listener->peers[i].open) count += 1;
  // tcp_info("%d open connections", count);
  
  peer->parent = listener;
  
  peer->fd = accept(listener->fd, (struct sockaddr*)&(peer->sockaddr), &len);
  if(peer->fd < 0) {
    tcp_error("Could not get peer socket");
    goto error;
  }
  
  
  int r = fcntl(peer->fd, F_SETFL, O_NONBLOCK);
  assert(r >= 0);
  
  peer->read_watcher.data = peer;
  ev_init(&(peer->read_watcher), tcp_peer_on_readable);
  ev_io_set(&(peer->read_watcher), peer->fd, EV_READ | EV_ERROR);
  ev_io_start(listener->loop, &(peer->read_watcher));
  
  peer->timeout_watcher.data = peer;
  ev_timer_init(&(peer->timeout_watcher), tcp_peer_on_timeout, TCP_TIMEOUT, TCP_TIMEOUT);
  ev_timer_start(listener->loop, &(peer->timeout_watcher));
  
  return peer;
  
error:
  tcp_peer_close(peer);
  return NULL;
}

void tcp_peer_stop_read_watcher(tcp_peer *peer)
{
  //assert(peer->open);
  assert(peer->parent->open);
  //assert(peer->read_watcher);
  ev_io_stop(peer->parent->loop, &(peer->read_watcher));
  //g_debug("killing read watcher");
  
}

void tcp_peer_close(tcp_peer *peer)
{
  if(peer->open) {
    ev_io_stop(peer->parent->loop, &(peer->read_watcher));
    ev_timer_stop(peer->parent->loop, &(peer->timeout_watcher));
    close(peer->fd);
    peer->open = FALSE;
  }
}

tcp_listener* tcp_listener_new(struct ev_loop *loop)
{
  int r;
  
  /* note the following will automatically set all of the peer structures
   * to have peer->open == FALSE */
  tcp_listener *listener = g_new0(tcp_listener, 1);
  
  listener->fd = socket(PF_INET, SOCK_STREAM, 0);
  r = fcntl(listener->fd, F_SETFL, O_NONBLOCK);
  assert(r >= 0);
  
  int flags = 1;
  r = setsockopt(listener->fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
  assert(r >= 0);
  
  listener->loop = loop;
  listener->open = FALSE;
  
  return listener;
}

void tcp_listener_free(tcp_listener *listener)
{
  tcp_listener_close(listener);
  free(listener);
  
  g_debug("tcp listener freed.");
}

void tcp_listener_close(tcp_listener *listener)
{
  int i;
  
  g_debug("close listener\n");
  assert(listener->open);
  
  tcp_peer *peer;
  for(i=0; i < TCP_MAX_PEERS; i++)
    tcp_peer_close(peer);
  
  if(listener->port_s) {
    free(listener->port_s);
    listener->port_s = NULL;
  }
  if(listener->dns_info) {
    free(listener->dns_info);
    listener->dns_info = NULL;
  }
  if(listener->accept_watcher) {
    printf("killing accept watcher\n");
    ev_io_stop(listener->loop, listener->accept_watcher);
    free(listener->accept_watcher);
    listener->accept_watcher = NULL;
  }
  
  close(listener->fd);
  listener->open = FALSE;
}

void tcp_listener_accept( struct ev_loop *loop
                        , struct ev_io *watcher
                        , int revents
                        )
{
  tcp_listener *listener = (tcp_listener*)(watcher->data);
  tcp_peer *peer;
  
  assert(listener->open);
  assert(listener->loop == loop);
  
  // check for error in revents
  if(EV_ERROR & revents) {
    tcp_error("tcp_peer_on_readable() got error event, closing free");
    tcp_listener_free(listener);
    return;
  }
  
  peer = tcp_peer_new(listener);
  
  if(listener->accept_cb != NULL && peer)
    listener->accept_cb(peer, listener->accept_cb_data);
  
  return;
}

void tcp_listener_listen ( tcp_listener *listener
                         , char *address
                         , int port
                         , tcp_listener_accept_cb_t accept_cb
                         , void *accept_cb_data
                         )
{
  int r;
  
  listener->sockaddr.sin_family = AF_INET;
  listener->sockaddr.sin_port = htons(port);
  
  /* for easy access to the port */
  listener->port_s = malloc(sizeof(char)*8);
  sprintf(listener->port_s, "%d", port);
  
  listener->dns_info = gethostbyname(address);
  if (!(listener->dns_info && listener->dns_info->h_addr)) {
    tcp_error("Could not look up hostname %s", address);
    goto error;
  }
  memmove(&(listener->sockaddr.sin_addr), listener->dns_info->h_addr, sizeof(struct in_addr));
  
  /* Other socket options. These could probably be fine tuned.
   * SO_SNDBUF       set buffer size for output
   * SO_RCVBUF       set buffer size for input
   * SO_SNDLOWAT     set minimum count for output
   * SO_RCVLOWAT     set minimum count for input
   * SO_SNDTIMEO     set timeout value for output
   * SO_RCVTIMEO     set timeout value for input
   */
  r = bind(listener->fd, (struct sockaddr*)&(listener->sockaddr), sizeof(listener->sockaddr));
  if(r < 0) {
    tcp_error("Failed to bind to %s %d", address, port);
    goto error;
  }

  r = listen(listener->fd, TCP_MAX_PEERS);
  if(r < 0) {
    tcp_error("listen() failed");
    goto error;
  }
  
  assert(listener->open == FALSE);
  listener->open = TRUE;
  
  listener->accept_watcher = g_new0(struct ev_io, 1);
  listener->accept_watcher->data = listener;
  listener->accept_cb = accept_cb;
  listener->accept_cb_data = accept_cb_data;
  
  ev_init (listener->accept_watcher, tcp_listener_accept);
  ev_io_set (listener->accept_watcher, listener->fd, EV_READ | EV_ERROR);
  ev_io_start (listener->loop, listener->accept_watcher);
  
  return;
error:
  tcp_listener_close(listener);
  return;
}

char* tcp_listener_address(tcp_listener *listener)
{
  if(listener->dns_info)
    return listener->dns_info->h_name;
  else
    return NULL;
}
