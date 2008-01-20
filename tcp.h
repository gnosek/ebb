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

typedef struct tcp_peer tcp_peer;
typedef struct tcp_listener tcp_listener;

#define TCP_CHUNKSIZE (16*1024)
#define TCP_MAX_PEERS 950
#define TCP_TIMEOUT 30.0

#define TCP_LOG_DOMAIN "TCP"
#define tcp_error(str, ...)  \
  g_log(TCP_LOG_DOMAIN, G_LOG_LEVEL_ERROR, str, ## __VA_ARGS__);
#define tcp_warning(str, ...)  \
  g_log(TCP_LOG_DOMAIN, G_LOG_LEVEL_WARNING, str, ## __VA_ARGS__);
#define tcp_info(str, ...)  \
  g_log(TCP_LOG_DOMAIN, G_LOG_LEVEL_INFO, str, ## __VA_ARGS__);
  
#define TCP_COMMON              \
  unsigned open : 1;            \
  int fd;                       \
  struct sockaddr_in sockaddr;

/*** TCP Peer ***/
typedef void (*tcp_peer_read_cb_t)(char *buffer, int length, void *data);

void tcp_peer_close(tcp_peer*);
int tcp_peer_write(tcp_peer *, const char *data, int length);

struct tcp_peer {
  TCP_COMMON
  
  tcp_listener *parent;
  
  void *read_cb_data;
  tcp_peer_read_cb_t read_cb;
  char read_buffer[TCP_CHUNKSIZE];
  ev_io read_watcher;
  
  // ev_io write_watcher;
  // char (*write_buffers)[100];
  // struct {
  //   int index;
  //   char *position;
  // } current_write_buffer;
  //tcp_peer_buffer_destroy_db buffer_destroy_cb;
  
  ev_timer timeout_watcher;
};


/*** TCP Listener ***/
typedef void (*tcp_listener_accept_cb_t) (tcp_peer *, void *callback_data);

// tcp_listener* tcp_listener_alloc();
// void tcp_listener_init(tcp_listener*
//                       , struct ev_loop *loo
//                       , char *address
//                       , int port
//                       , tcp_listener_accept_cb_t
//                       , void *accept_cb_data
//                       );
tcp_listener* tcp_listener_new(struct ev_loop *loop);
void tcp_listener_free(tcp_listener*);
void tcp_listener_close(tcp_listener*);
void tcp_listener_listen( tcp_listener*
                        , char *address
                        , int port
                        , tcp_listener_accept_cb_t
                        , void *accept_cb_data
                        );
char* tcp_listener_address(tcp_listener*);

struct tcp_listener {
  TCP_COMMON
  struct hostent *dns_info;
  char *port_s;
  
  void *accept_cb_data;
  tcp_listener_accept_cb_t accept_cb;
  
  ev_io *accept_watcher;
  struct ev_loop *loop;
  
  struct tcp_peer peers[TCP_MAX_PEERS];
};

#endif tcp_h