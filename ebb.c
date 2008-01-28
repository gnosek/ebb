/* Ebb Web Server
 * Copyright (c) 2007 Ry Dahl
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
#include <assert.h>

#include <glib.h>
#include <ev.h>

#include "mongrel/parser.h"

#include "ebb.h"
#include "parser_callbacks.h"

void ebb_dispatch(ebb_client *client)
{
  ebb_server *server = client->server;
  
  assert(client->open);
  
  /* Set the env variables */
  ebb_client_add_env_const(client, EBB_SERVER_NAME
                                 , server->address
                                 , strlen(server->address)
                                 );
  ebb_client_add_env_const(client, EBB_SERVER_PORT
                                 , server->port
                                 , strlen(server->port)
                                 );
  
  server->request_cb(client, server->request_cb_data);
}

void ebb_on_timeout( struct ev_loop *loop
                   , ev_timer *watcher
                   , int revents
                   )
{
  ebb_client *client = (ebb_client*)(watcher->data);
  
  assert(client->server->loop == loop);
  assert(&(client->timeout_watcher) == watcher);
  
  ebb_client_close(client);
  ebb_info("peer timed out");
}

void ebb_on_readable( struct ev_loop *loop
                    , ev_io *watcher
                    , int revents
                    )
{
  ebb_client *client = (ebb_client*)(watcher->data);
  
  assert(client->open);
  assert(client->server->open);
  assert(client->server->loop == loop);
  assert(&(client->read_watcher) == watcher);
  assert(!http_parser_is_finished(&(client->parser)));
  
  if(EV_ERROR & revents) {
    ebb_error("ebb_on_readable() got error event, closing peer");
    goto error;
  }
  
  ssize_t read = recv( client->fd
                     , client->read_buffer + client->read
                     , EBB_CHUNKSIZE - client->read - 1
                     , 0
                     );
  if(read == 0) {
    ebb_client_close(client); /* XXX is this the right action to take? */
    return;
  } else if(read < 0) {
    if(errno == EBADF || errno == ECONNRESET)
      g_debug("Connection reset by peer");
    else
      ebb_error("Error recving data: %s", strerror(errno));
    goto error;
  }
  
  ev_timer_again(loop, &(client->timeout_watcher));
#ifdef DEBUG
  g_debug("Read %d bytes", (int)read);
#endif
  client->read += read;
  if(EBB_CHUNKSIZE < client->read) {
    // TODO: File upload use the GString buffer
    //g_string_append_len(client->buffer, client->read_buffer, read);
    assert(FALSE);
  } 
  
  // make ragel happy and put a nul character at the end of the stream
  // we will write over this in the next iteration
  client->read_buffer[client->read + 1] = '\0';
  
  http_parser_execute( &(client->parser)
                     , client->read_buffer
                     , client->read + 1
                     , client->parser.nread
                     );
  if(http_parser_is_finished(&(client->parser))) {
    ev_io_stop(loop, watcher);
    ebb_dispatch(client);
  }
  
  return;
error:
  ebb_client_close(client);
}

void ebb_on_request( struct ev_loop *loop
                   , ev_io *watcher
                   , int revents
                   )
{
  ebb_server *server = (ebb_server*)(watcher->data);
  
  assert(server->open);
  assert(server->loop == loop);
  assert(server->request_watcher == watcher);
  
  if(EV_ERROR & revents) {
    ebb_info("ebb_on_request() got error event, closing server.");
    ebb_server_stop(server);
    return;
  }
  
  /* Now we're going to initialize the client 
   * and set up her callbacks for read and write
   * the client won't get passed back to the user, however,
   * until the request is complete and parsed.
   */
  
  int i;
  ebb_client *client;
  /* Get next availible peer */
  for(i=0; i < EBB_MAX_CLIENTS; i++)
    if(!server->clients[i].open) {
      client = &(server->clients[i]);
      break;
    }
  if(client == NULL) {
    g_message("Too many peers. Refusing connections.");
    return;
  }
  client->open = TRUE;
  
  /* DEBUG
  int count = 0;
  for(i = 0; i < EBB_MAX_CLIENTS; i++)
    if(server->clients[i].open) count += 1;
  tcp_info("%d open connections", count);
  */
  
  /* DO SOCKET STUFF */
  socklen_t len;
  client->fd = accept(server->fd, (struct sockaddr*)&(server->sockaddr), &len);
  assert(client->fd >= 0);
  // int r = fcntl(client->fd, F_SETFL, O_NONBLOCK);
  // assert(r >= 0);
  
  /* INITIALIZE inline http_parser */
  http_parser_init(&(client->parser));
  client->parser.data = client;
  client->parser.http_field     = http_field_cb;
  client->parser.request_method = request_method_cb;
  client->parser.request_uri    = request_uri_cb;
  client->parser.fragment       = fragment_cb;
  client->parser.request_path   = request_path_cb;
  client->parser.query_string   = query_string_cb;
  client->parser.http_version   = http_version_cb;
  client->parser.header_done    = header_done_cb;
  
  /* OTHER */
  client->env_size = 0;
  client->read = 0;
  client->server = server;
  
  /* SETUP READ AND TIMEOUT WATCHERS */
  client->read_watcher.data = client;
  client->timeout_watcher.data = client;
  ev_init(&(client->read_watcher), ebb_on_readable);
  ev_timer_init(&(client->timeout_watcher), ebb_on_timeout, EBB_TIMEOUT, EBB_TIMEOUT);
  ev_io_set(&(client->read_watcher), client->fd, EV_READ | EV_ERROR);
  ev_io_start(server->loop, &(client->read_watcher));
  ev_timer_start(server->loop, &(client->timeout_watcher));
}

ebb_server* ebb_server_alloc()
{
  ebb_server *server = g_new0(ebb_server, 1);
  return server;
}

void ebb_server_init( ebb_server *server
                    , struct ev_loop *loop
                    , char *address
                    , int port
                    , ebb_request_cb request_cb
                    , void *request_cb_data
                    )
{
  /* SETUP SOCKET STUFF */
  server->fd = socket(PF_INET, SOCK_STREAM, 0);
  int r;
  // r = fcntl(server->fd, F_SETFL, O_NONBLOCK);
  // assert(r >= 0);
  int flags = 1;
  r = setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
  assert(r >= 0);
  server->sockaddr.sin_family = AF_INET;
  server->sockaddr.sin_port = htons(port);
  server->address = strdup(address); /* TODO: use looked up address? */
  server->port = malloc(sizeof(char)*8); /* for easy access to the port */
  sprintf(server->port, "%d", port);
  server->dns_info = gethostbyname(address);
  if (!(server->dns_info && server->dns_info->h_addr)) {
    ebb_error("Could not look up hostname %s", address);
    goto error;
  }
  memmove( &(server->sockaddr.sin_addr)
         , server->dns_info->h_addr
         , sizeof(struct in_addr)
         );
  
  server->request_cb = request_cb;
  server->request_cb_data = request_cb_data;
  server->loop = loop;
  server->open = FALSE;
  return;
error:
  ebb_server_free(server);
  return;
}


void ebb_server_free(ebb_server *server)
{
  ebb_server_stop(server);
  if(server->port)
    free(server->port);
  if(server->address)
    free(server->address);
  if(server->dns_info)
    free(server->dns_info);
  free(server);
}

void ebb_server_stop(ebb_server *server)
{
  g_debug("stopping ebb server\n");
  assert(server->open);
  
  int i;
  ebb_client *client;
  for(i=0; i < EBB_MAX_CLIENTS; i++)
    ebb_client_close(client);
  
  if(server->request_watcher) {
    printf("killing request watcher\n");
    ev_io_stop(server->loop, server->request_watcher);
    free(server->request_watcher);
    server->request_watcher = NULL;
  }
  
  close(server->fd);
  server->open = FALSE;
}

void ebb_server_start(ebb_server *server)
{
  int r = bind( server->fd
              , (struct sockaddr*)&(server->sockaddr)
              , sizeof(server->sockaddr)
              );
  if(r < 0) {
    ebb_error("Failed to bind to %s %s", server->address, server->port);
    ebb_server_stop(server);
    return;
  }
  r = listen(server->fd, EBB_MAX_CLIENTS);
  assert(r >= 0);
  assert(server->open == FALSE);
  server->open = TRUE;
  
  server->request_watcher = g_new0(ev_io, 1);
  server->request_watcher->data = server;
  
  ev_init (server->request_watcher, ebb_on_request);
  ev_io_set (server->request_watcher, server->fd, EV_READ | EV_ERROR);
  ev_io_start (server->loop, server->request_watcher);
}

void ebb_client_close(ebb_client *client)
{
  if(client->open) {
    ev_io_stop(client->server->loop, &(client->read_watcher));
    ev_io_stop(client->server->loop, &(client->write_watcher));
    ev_timer_stop(client->server->loop, &(client->timeout_watcher));
    
    /* http_parser */
    http_parser_finish(&(client->parser));
    
    // /* buffer */
    // if(client->upload_buffer)
    //   g_string_free(client->upload_buffer, TRUE);
    
    close(client->fd);
    client->open = FALSE;
  }
}

/* TODO replace this with callback write */
int ebb_client_write(ebb_client *client, const char *data, int length)
{
  int total_sent=0, sent;
  if(!client->open) {
    //tcp_warning("Trying to write to a peer that isn't open.");
    return 0;
  }
   
  while(total_sent < length) {
    sent = send( client->fd
               , data + sizeof(char)*total_sent
               , length - total_sent
               , 0
               );
    if(sent < 0) {
      //ebb_warning("Error writing: %s", strerror(errno));
      ebb_client_close(client);
      break;
    }
    total_sent += sent;
    ev_timer_again(client->server->loop, &(client->timeout_watcher));
  }
  return total_sent;
}

void ebb_on_writable( struct ev_loop *loop
                    , ev_io *watcher
                    , int revents
                    )
{
  ebb_client *client = (ebb_client*)(watcher->data);
  ssize_t sent;
  
  if(client->written != 0)
    ebb_info("total written: %d", (int)(client->written));
  
  sent = send( client->fd
             , client->write_buffer + sizeof(char)*(client->written)
             , client->write_buffer_len - client->written
             , 0
             );
  if(sent < 0) {
    // ebb_warning("Error writing: %s", strerror(errno));
    goto stop_writer;
  }
  client->written += sent;
  
  //ebb_info("wrote %d bytes. total: %d", (int)sent, (int)(client->written));
  
  ev_timer_again(loop, &(client->timeout_watcher));
  
  if(client->written == client->write_buffer_len) {
    goto stop_writer;
  }
  return;
stop_writer:
  ev_io_stop(loop, watcher);
  client->after_write_cb(client, client->write_cb_data);
}

void ebb_client_evented_write( ebb_client *client
                             , const char *data
                             , int length
                             , ebb_after_write_cb after_write_cb
                             , void *write_cb_data
                             )
{
  assert(client->open);
  assert(FALSE == ev_is_active(&(client->write_watcher)));
  
  client->write_buffer = data;
  client->write_buffer_len = length;
  client->written = 0;
  client->write_cb_data = write_cb_data;
  client->after_write_cb = after_write_cb;
  client->write_watcher.data = client;
  ev_init (&(client->write_watcher), ebb_on_writable);
  ev_io_set (&(client->write_watcher), client->fd, EV_WRITE | EV_ERROR);
  ev_io_start (client->server->loop, &(client->write_watcher));
}

