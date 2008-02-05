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

#define ebb_env_add(client, field,flen,value,vlen)                          \
  client->env_fields[client->env_size] = field;                             \
  client->env_field_lengths[client->env_size] = flen;                       \
  client->env_values[client->env_size] = value;                             \
  client->env_value_lengths[client->env_size] = vlen;                       \
  client->env_size += 1;                                                     
#define ebb_env_add_const(client,field,value,vlen)                          \
  client->env_fields[client->env_size] = NULL;                              \
  client->env_field_lengths[client->env_size] = field;                      \
  client->env_values[client->env_size] = value;                             \
  client->env_value_lengths[client->env_size] = vlen;                       \
  client->env_size += 1;                                                     
#define ebb_env_error(client)                                               \
  client->env_fields[client->env_size] = NULL;                              \
  client->env_field_lengths[client->env_size] = -1;                         \
  client->env_values[client->env_size] = NULL;                              \
  client->env_value_lengths[client->env_size] = -1;                         \
  client->env_size += 1;                                                     

#include "parser_callbacks.h"

int ebb_env_has_error(ebb_client *client)
{
  int i;
  for(i = 0; i < client->env_size; i++)
    if(client->env_field_lengths[i] < 0)
      return TRUE;
  return FALSE;
}

void ebb_client_set_nonblocking(ebb_client *client)
{
  int flags = fcntl(client->fd, F_GETFL, 0);
  assert(0 <= fcntl(client->fd, F_SETFL, flags | O_NONBLOCK));
}

void ebb_client_set_blocking(ebb_client *client)
{
  int flags = fcntl(client->fd, F_GETFL, 0);
  assert(0 <= fcntl(client->fd, F_SETFL, flags & ~O_NONBLOCK));
}

void ebb_dispatch(ebb_client *client)
{
  ebb_server *server = client->server;
  
  assert(client->open);
  
  if(ebb_env_has_error(client)) {
    ebb_client_close(client);
    return;
  }
  
  /* Set the env variables */
  ebb_env_add_const(client, EBB_SERVER_NAME
                                 , server->address
                                 , strlen(server->address)
                                 );
  ebb_env_add_const(client, EBB_SERVER_PORT
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
#ifdef DEBUG
  ebb_info("peer timed out");
#endif
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
    //ebb_warning("read zero from client?");
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
    client->input_head = client->read_buffer + client->parser.nread;
    client->input_head_len = client->read - client->parser.nread;
    client->input_read = 0;
    
    ev_io_stop(loop, watcher);
    
    ebb_dispatch(client);
  }
  
  return;
error:
  ebb_client_close(client);
}

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define ramp(a) (a > 0 ? a : 0)
GString* ebb_client_read_input(ebb_client *client, ssize_t asked_to_read)
{
  assert(client->content_length >= 0);
  assert(client->input_read >= 0);
  assert(client->open);
  assert(client->server->open);
  assert(http_parser_is_finished(&(client->parser)));
  
  ssize_t to_read = min(asked_to_read, client->content_length - client->input_read);
  GString *string = g_string_sized_new(to_read);
  
  assert(to_read >= 0);
  
  ssize_t to_read_from_head = ramp(min(to_read, client->input_head_len - client->input_read));
  ssize_t to_read_from_socket = to_read - to_read_from_head;
  
#ifdef DEBUG
  ebb_debug("to_read: %d", (int)to_read);
  ebb_debug("to_read_from_head: %d", (int)to_read_from_head);
  ebb_debug("to_read_from_socket: %d", (int)to_read_from_socket);
#endif

  assert(to_read_from_head >= 0);
  assert(to_read_from_socket >= 0);
  assert(to_read_from_head <= client->input_head_len);
  assert(to_read_from_head + to_read_from_socket == to_read);
  
  g_string_append_len( string
                     , client->input_head + client->input_read
                     , to_read_from_head
                     );
  client->input_read += to_read_from_head;
  
  ssize_t read_from_socket = recv( client->fd
                                 , string->str
                                 , to_read_from_socket
                                 , 0
                                 );
  if(read_from_socket < 0) {
    ebb_error("Problem reading from socket: %s", strerror(errno));
  } else {
    string->len += read_from_socket;
    client->input_read += read_from_socket;
  }
  
  return string;
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
  
#ifdef DEBUG
  int count = 0;
  for(i = 0; i < EBB_MAX_CLIENTS; i++)
    if(server->clients[i].open) count += 1;
  ebb_debug("%d open connections", count);
#endif
  
  /* DO SOCKET STUFF */
  socklen_t len;
  client->fd = accept(server->fd, (struct sockaddr*)&(server->sockaddr), &len);
  assert(client->fd >= 0);
  ebb_client_set_nonblocking(client);
  
  
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
  client->parser.content_length = content_length_cb;
  client->parser.header_done    = header_done_cb;
  
  /* OTHER */
  client->env_size = 0;
  client->read = 0;
  client->server = server;
  client->write_buffer->len = 0; // see note in ebb_client_close
  
  // for error detection
  client->input_read = -1;
  client->content_length = -1;
  
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
  
  int i;
  for(i=0; i < EBB_MAX_CLIENTS; i++)
    server->clients[i].write_buffer = g_string_new("");
  
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
  
  int i; 
  for(i=0; i < EBB_MAX_CLIENTS; i++)
    g_string_free(server->clients[i].write_buffer, TRUE);
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
  ebb_info("Stopping ebb server");
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
    
    /* buffer */
    /* the theory here is that we do not free the already allocated 
     * strings that we're holding the response in. we reuse it again - 
     * presumably because the backend is going to keep sending such long
     * requests.
     */
    client->write_buffer->len = 0;
    
    close(client->fd);
    client->open = FALSE;
  }
}

void ebb_on_writable( struct ev_loop *loop
                    , ev_io *watcher
                    , int revents
                    )
{
  ebb_client *client = (ebb_client*)(watcher->data);
  ssize_t sent;
  
  if(EV_ERROR & revents) {
    ebb_error("ebb_on_readable() got error event, closing peer");
    return;
  }
  
  //if(client->written != 0)
  //  ebb_debug("total written: %d", (int)(client->written));
  
  sent = send( client->fd
             , client->write_buffer->str + sizeof(gchar)*(client->written)
             , client->write_buffer->len - client->written
             , 0
             );
  if(sent < 0) {
#ifdef DEBUG
    ebb_warning("Error writing: %s", strerror(errno));
#endif
    ebb_client_close(client);
    return;
  }
  client->written += sent;
  
  assert(client->written <= client->write_buffer->len);
  //ebb_info("wrote %d bytes. total: %d", (int)sent, (int)(client->written));
  
  ev_timer_again(loop, &(client->timeout_watcher));
  
  if(client->written == client->write_buffer->len) {
    if(client->after_write_cb) client->after_write_cb(client);
    ebb_client_close(client);
  }
}

void ebb_client_write(ebb_client *client, const char *data, int length)
{
  g_string_append_len(client->write_buffer, data, length);
}

void ebb_client_start_writing( ebb_client *client
                             , ebb_client_cb after_write_cb
                             )
{
  assert(client->open);
  assert(FALSE == ev_is_active(&(client->write_watcher)));
  
  ebb_client_set_nonblocking(client);
  
  client->written = 0;
  client->after_write_cb = after_write_cb;
  client->write_watcher.data = client;
  ev_init (&(client->write_watcher), ebb_on_writable);
  ev_io_set (&(client->write_watcher), client->fd, EV_WRITE | EV_ERROR);
  ev_io_start(client->server->loop, &(client->write_watcher));
}
