#include "evtcp.h"


/*** TCP Server ***/
evtcp_server* evtcp_server_new( tcp_error_cb error_cb)
{
  evtcp_server *server = g_new0( evtcp_server, 1);
  
  server->max_clients = 1024;
  server->client_timeout = 60;
  server->clients = g_queue_new();
  server->socket = evtcp_socket_new(4096, error_cb);
  server->watcher = g_new0(ev_io, 1);
  server->watcher->data = (void*)server;
  server->loop = ev_default_init(EVFLAG_AUTO);
  return server;
}

void evtcp_server_free(evtcp_server*)
{
  g_queue_free( server->clients );
  
  evtcp_socket_free( server->socket );
  ev_io_stop ( server->watcher );
  free( server->watcher );
  // free the default loop? 
  // ev_default_destroy();
}

void evtcp_server_real_connection_cb( struct ev_loop *loop
                         , struct ev_io *watcher
                         , int revents
                         )
{
  evtcp_server *server = (evtcp_server*)(watcher->data);
  evtcp_socket *client_socket;
  evtcp_client *client;
  
  client_socket = evtcp_socket_accept(server->socket)
  client = evtcp_client_new(client_socket, server);
  g_queue_push_head(server->clients, (gpointer)client);
  server->connect_cb(server, client);
}

void evtcp_server_bind( evtcp_server *server
                      , char *address
                      , int port
                      , evtcp_connection_cb connect_cb
                      )
{ 
  evtcp_socket_listen(server->socket, address, port);
  server->connect_cb = connect_cb;
  ev_init( server->watcher, evtcp_server_real_connection_cb );
  ev_io_set( server->watcher, server->socket->fd, EV_READ );
  ev_io_start( loop, server->watcher );
  ev_loop(loop, 0);
}

/*** TCP Client ***/
evtcp_client* evtcp_client_new(evtcp_server *parent);
void evtcp_client_free(evtcp_client*);