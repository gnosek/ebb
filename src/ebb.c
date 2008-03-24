/* The Ebb Web Server
 * Copyright (c) 2008 Ry Dahl. This software is released under the MIT 
 * License. See README file for details.
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include <pthread.h>
#include <glib.h>

#define EV_STANDALONE 1
#include <ev.c>

#include "parser.h"
#include "ebb.h"

#define min(a,b) (a < b ? a : b)
#define ramp(a) (a > 0 ? a : 0)

static int server_socket_unix(const char *path, int access_mask);
static void client_init(ebb_client *client);

static void set_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  assert(0 <= fcntl(fd, F_SETFL, flags | O_NONBLOCK) && "Setting socket non-block failed!");
}


void env_add(ebb_client *client, const char *field, int flen, const char *value, int vlen)
{
  if(client->env_size >= EBB_MAX_ENV) {
    client->parser.overflow_error = TRUE;
    return;
  }
  client->env[client->env_size].type = -1;
  client->env[client->env_size].field = field;
  client->env[client->env_size].field_length = flen;
  client->env[client->env_size].value = value;
  client->env[client->env_size].value_length = vlen;
  client->env_size += 1;
}


void env_add_const(ebb_client *client, int type, const char *value, int vlen)
{
  if(client->env_size >= EBB_MAX_ENV) {
    client->parser.overflow_error = TRUE;
    return;
  }
  client->env[client->env_size].type = type;
  client->env[client->env_size].field = NULL;
  client->env[client->env_size].field_length = -1;
  client->env[client->env_size].value = value;
  client->env[client->env_size].value_length = vlen;
  client->env_size += 1;
}


void http_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  ebb_client *client = (ebb_client*)(data);
  assert(field != NULL);
  assert(value != NULL);
  env_add(client, field, flen, value, vlen);
}


void on_element(void *data, int type, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  env_add_const(client, type, at, length);
}


static void dispatch(ebb_client *client)
{
  ebb_server *server = client->server;
  if(client->open == FALSE)
    return;
  client->in_use = TRUE;
  
  /* decide if to use keep-alive or not */
  

  
  
  server->request_cb(client, server->request_cb_data);
}


static void on_timeout(struct ev_loop *loop, ev_timer *watcher, int revents)
{
  ebb_client *client = (ebb_client*)(watcher->data);
  
  assert(client->server->loop == loop);
  assert(&(client->timeout_watcher) == watcher);
  
  ebb_client_close(client);
#ifdef DEBUG
  g_message("peer timed out");
#endif
}

#define client_finished_parsing http_parser_is_finished(&client->parser)
#define total_request_size (client->parser.content_length + client->parser.nread)

static void* read_body_into_file(void *_client)
{
  ebb_client *client = (ebb_client*)_client;
  static unsigned int id;
  FILE *tmpfile;
  
  assert(client->open);
  assert(client->server->open);
  assert(client->parser.content_length > 0);
  assert(client_finished_parsing);
  
  /* set blocking socket */
  int flags = fcntl(client->fd, F_GETFL, 0);
  assert(0 <= fcntl(client->fd, F_SETFL, flags & ~O_NONBLOCK));
  
  sprintf(client->upload_filename, "/tmp/ebb_upload_%010d", id++);
  tmpfile = fopen(client->upload_filename, "w+");
  if(tmpfile == NULL) g_message("Cannot open tmpfile %s", client->upload_filename);
  client->upload_file = tmpfile;
  
  size_t body_head_length = client->read - client->parser.nread;
  size_t written = 0, r;
  while(written < body_head_length) {
    r = fwrite( client->request_buffer + sizeof(char)*(client->parser.nread + written)
              , sizeof(char)
              , body_head_length - written
              , tmpfile
              );
    if(r <= 0) {
      ebb_client_close(client);
      return NULL;
    }
    written += r;
  }
  
  int bufsize = 5*1024;
  char buffer[bufsize];
  size_t received;
  while(written < client->parser.content_length) {
    received = recv(client->fd
                   , buffer
                   , min(client->parser.content_length - written, bufsize)
                   , 0
                   );
    if(received < 0) goto error;
    client->read += received;
    
    ssize_t w = 0;
    int rv;
    while(w < received) {
      rv = fwrite( buffer + w*sizeof(char)
                 , sizeof(char)
                 , received - w
                 , tmpfile
                 );
      if(rv <= 0) goto error;
      w += rv;
    }
    written += received;
  }
  rewind(tmpfile);
  // g_debug("%d bytes written to file %s", written, client->upload_filename);
  dispatch(client);
  return NULL;
error:
  ebb_client_close(client);
  return NULL;
}


static void on_client_readable(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_client *client = (ebb_client*)(watcher->data);
  
  assert(client->in_use == FALSE);
  assert(client->open);
  assert(client->server->open);
  assert(client->server->loop == loop);
  assert(&client->read_watcher == watcher);
  
  ssize_t read = recv( client->fd
                     , client->request_buffer + client->read
                     , EBB_BUFFERSIZE - client->read
                     , 0
                     );
  if(read < 0) goto error;
  if(read == 0) goto error; /* XXX is this the right action to take for read==0 ? */
  client->read += read;
  ev_timer_again(loop, &client->timeout_watcher);
  
  if(client->read == EBB_BUFFERSIZE) goto error;
  
  if(FALSE == client_finished_parsing) {
    http_parser_execute( &client->parser
                       , client->request_buffer
                       , client->read
                       , client->parser.nread
                       );
    if(http_parser_has_error(&client->parser)) goto error;
  }
  
  if(client_finished_parsing) {
    if(total_request_size == client->read) {
      ev_io_stop(loop, watcher);
      client->nread_from_body = 0;
      dispatch(client);
      return;
    }
    if(total_request_size > EBB_BUFFERSIZE ) {
      /* read body into file - in a thread */
      ev_io_stop(loop, watcher);
      pthread_t thread;
      assert(0 <= pthread_create(&thread, NULL, read_body_into_file, client));
      pthread_detach(thread);
      return;
    }
  }
  return;
error:
#ifdef DEBUG
  if(read < 0) g_message("Error recving data: %s", strerror(errno));
#endif
  ebb_client_close(client);
}


static void on_client_writable(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_client *client = (ebb_client*)(watcher->data);
  ssize_t sent;
  
  if(client->status_written == FALSE || client->headers_written == FALSE) {
    g_message("no status or headers - closing connection.");
    goto error;
  }
  
  if(EV_ERROR & revents) {
    g_message("on_client_writable() got error event, closing peer");
    goto error;
  }
  
  //if(client->written != 0)
  //  g_debug("total written: %d", (int)(client->written));
  
  sent = send( client->fd
             , client->response_buffer->str + sizeof(gchar)*(client->written)
             , client->response_buffer->len - client->written
             , 0
             );
  if(sent < 0) {
#ifdef DEBUG
    g_message("Error writing: %s", strerror(errno));
#endif
    goto error;
  } else if(sent == 0) {
    /* is this the wrong thing to do? */
    g_message("Sent zero bytes? Closing connection");
    goto error;
  }
  client->written += sent;
  
  assert(client->written <= client->response_buffer->len);
  //g_message("wrote %d bytes. total: %d", (int)sent, (int)(client->written));
  
  ev_timer_again(loop, &client->timeout_watcher);
  
  if(client->written == client->response_buffer->len) {
    /* stop the write watcher. to be restarted by the next call to ebb_client_write_body
     * or if client->body_written is set (by using ebb_client_release) then
     * we close the connection
     */
    ev_io_stop(loop, watcher);
    if(client->body_written) {
      client->keep_alive ? client_init(client) : ebb_client_close(client);
    }
  }
  return;
error:
  ebb_client_close(client);
}


static void client_init(ebb_client *client)
{
  assert(client->in_use == FALSE);
  
  /* If the client is already open, reuse the fd, just reset all the parameters
   * this would happen in the case of a keep_alive request
   */
  if(!client->open) {
    /* DO SOCKET STUFF */
    socklen_t len;
    int fd = accept(client->server->fd, (struct sockaddr*)&(client->server->sockaddr), &len);
    if(fd < 0) {
      perror("accept()");
      return;
    }
    client->open = TRUE;
    client->fd = fd;
  }
  
  set_nonblock(client->fd);
  
  /* INITIALIZE http_parser */
  http_parser_init(&client->parser);
  client->parser.data = client;
  client->parser.http_field = http_field_cb;
  client->parser.on_element = on_element;
  
  /* OTHER */
  client->env_size = 0;
  client->read = client->nread_from_body = 0;
  if(client->request_buffer == NULL) {
    /* Only allocate the request_buffer once */
    client->request_buffer = (char*)malloc(EBB_BUFFERSIZE);
  }
  client->keep_alive = FALSE;
  client->status_written = client->headers_written = client->body_written = FALSE;
  client->written = 0;
  /* here we do not free the already allocated GString client->response_buffer
   * that we're holding the response in. we reuse it again - presumably 
   * because the backend is going to keep sending such long requests.
   */
  client->response_buffer->len = 0;
  
  /* SETUP READ AND TIMEOUT WATCHERS */
  client->write_watcher.data = client;
  ev_init (&client->write_watcher, on_client_writable);
  ev_io_set (&client->write_watcher, client->fd, EV_WRITE | EV_ERROR);
  /* Note, do not start write_watcher until there is something to be written.
   * See ebb_client_write_body() */
  
  client->read_watcher.data = client;
  ev_init(&client->read_watcher, on_client_readable);
  ev_io_set(&client->read_watcher, client->fd, EV_READ | EV_ERROR);
  ev_io_start(client->server->loop, &client->read_watcher);
  
  client->timeout_watcher.data = client;  
  ev_timer_init(&client->timeout_watcher, on_timeout, EBB_TIMEOUT, EBB_TIMEOUT);
  ev_timer_start(client->server->loop, &client->timeout_watcher);
}


static void on_request(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_server *server = (ebb_server*)(watcher->data);
  assert(server->open);
  assert(server->loop == loop);
  assert(&server->request_watcher == watcher);
  
  if(EV_ERROR & revents) {
    g_message("on_request() got error event, closing server.");
    ebb_server_unlisten(server);
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
    if(!server->clients[i].in_use && !server->clients[i].open) {
      client = &(server->clients[i]);
      break;
    }
  if(client == NULL) {
    g_message("Too many peers. Refusing connections.");
    return;
  }
  
#ifdef DEBUG
  int count = 0;
  for(i = 0; i < EBB_MAX_CLIENTS; i++)
    if(server->clients[i].open) count += 1;
  g_debug("%d open connections", count);
#endif
  
  client_init(client);
}


ebb_server* ebb_server_alloc()
{
  ebb_server *server = g_new0(ebb_server, 1);
  return server;
}


void ebb_server_init( ebb_server *server
                    , struct ev_loop *loop
                    , ebb_request_cb request_cb
                    , void *request_cb_data
                    )
{
  int i;
  for(i=0; i < EBB_MAX_CLIENTS; i++) {
    server->clients[i].request_buffer = NULL;
    server->clients[i].response_buffer = g_string_new("");
    server->clients[i].open = FALSE;
    server->clients[i].in_use = FALSE;
    server->clients[i].server = server;
  }
  
  server->request_cb = request_cb;
  server->request_cb_data = request_cb_data;
  server->loop = loop;
  server->open = FALSE;
  server->fd = -1;
  return;
error:
  ebb_server_free(server);
  return;
}


void ebb_server_free(ebb_server *server)
{
  ebb_server_unlisten(server);
  
  int i; 
  for(i=0; i < EBB_MAX_CLIENTS; i++)
    g_string_free(server->clients[i].response_buffer, TRUE);
  if(server->port)
    free(server->port);
  if(server->socketpath)
    free(server->socketpath);
  free(server);
}


void ebb_server_unlisten(ebb_server *server)
{
  if(server->open) {
    int i;
    ebb_client *client;
    ev_io_stop(server->loop, &server->request_watcher);
    close(server->fd);
    if(server->socketpath)
      unlink(server->socketpath);
    server->open = FALSE;
  }
}

int ebb_server_listen_on_port(ebb_server *server, const int port)
{
  int sfd = -1;
  struct linger ling = {0, 0};
  struct sockaddr_in addr;
  int flags = 1;
  
  if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket()");
    goto error;
  }
  
  set_nonblock(sfd);
  
  flags = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
  setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
  setsockopt(sfd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
  setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
  
  /*
   * the memset call clears nonstandard fields in some impementations
   * that otherwise mess things up.
   */
  memset(&addr, 0, sizeof(addr));
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind()");
    goto error;
  }
  if (listen(sfd, EBB_MAX_CLIENTS) < 0) {
    perror("listen()");
    goto error;
  }
  server->fd = sfd;
  server->port = malloc(sizeof(char)*8); /* for easy access to the port */
  sprintf(server->port, "%d", port);
  assert(server->open == FALSE);
  server->open = TRUE;
  
  server->request_watcher.data = server;
  ev_init (&server->request_watcher, on_request);
  ev_io_set (&server->request_watcher, server->fd, EV_READ | EV_ERROR);
  ev_io_start (server->loop, &server->request_watcher);
  
  return server->fd;
error:
  if(sfd > 0) close(sfd);
  return -1;
}


int ebb_server_listen_on_socket(ebb_server *server, const char *socketpath)
{
  // int fd = server_socket_unix(socketpath, 0755);
  // if(fd < 0) return 0;
  // server->socketpath = strdup(socketpath);
  // server->fd = fd;
  // server_listen(server);
  // return fd;
}


void ebb_client_release(ebb_client *client)
{
  assert(client->in_use);
  client->in_use = FALSE;
  client->body_written = TRUE;
  if(client->written == client->response_buffer->len)
    ebb_client_close(client);
}


void ebb_client_close(ebb_client *client)
{
  if(client->open) {
    ev_io_stop(client->server->loop, &client->read_watcher);
    ev_io_stop(client->server->loop, &client->write_watcher);
    ev_timer_stop(client->server->loop, &client->timeout_watcher);
    
    if(client->upload_file) {
      fclose(client->upload_file);
      unlink(client->upload_filename);
    }
    
    close(client->fd);
    client->open = FALSE;
  }
}


void ebb_client_write_status(ebb_client *client, int status, const char *human_status)
{
  assert(client->in_use);
  if(!client->open) return;
  assert(client->status_written == FALSE);
  g_string_append_printf( client->response_buffer
                        , "HTTP/1.1 %d %s\r\n"
                        , status
                        , human_status
                        );
  client->status_written = TRUE;
}


void ebb_client_write_header(ebb_client *client, const char *field, const char *value)
{
  assert(client->in_use);
  if(!client->open) return;
  assert(client->status_written == TRUE);
  assert(client->headers_written == FALSE);
  
  if(strcmp(field, "Connection") == 0 && strcmp(value, "Keep-Alive") == 0) {
    client->keep_alive = TRUE;
  }
  g_string_append_printf( client->response_buffer
                        , "%s: %s\r\n"
                        , field
                        , value
                        );
}


void ebb_client_write_body(ebb_client *client, const char *data, int length)
{
  assert(client->in_use);
  if(!client->open) return;
  
  if(client->headers_written == FALSE) {
    g_string_append(client->response_buffer, "\r\n");
    client->headers_written = TRUE;
  }
  
  g_string_append_len(client->response_buffer, data, length);
  
  /* If the write_watcher isn't yet active, then start it. It could be that
   * we're streaming and the watcher has been stopped. In that case we 
   * start it again since we have more to write. */
  if(ev_is_active(&client->write_watcher) == FALSE) {
    set_nonblock(client->fd);
    ev_io_start(client->server->loop, &client->write_watcher);
  }
}


/* pass an allocated buffer and the length to read. this function will try to
 * fill the buffer with that length of data read from the body of the request.
 * the return value says how much was actually written.
 */
int ebb_client_read(ebb_client *client, char *buffer, int length)
{
  size_t read;
  
  assert(client->in_use);
  if(!client->open) return -1;
  assert(client_finished_parsing);
  
  if(client->upload_file) {
    read = fread(buffer, 1, length, client->upload_file);
    /* TODO error checking! */
    return read;
  } else {
    char* request_body = client->request_buffer + client->parser.nread;
    
    read = ramp(min(length, client->parser.content_length - client->nread_from_body));
    memcpy( buffer
          , request_body + client->nread_from_body
          , read
          );
    client->nread_from_body += read;
    return read;
  }
}

/* The following socket creation routines are modified and stolen from memcached */


static int server_socket_unix(const char *path, int access_mask) {
    int sfd;
    struct linger ling = {0, 0};
    struct sockaddr_un addr;
    struct stat tstat;
    int flags =1;
    int old_umask;

    if (!path) {
        return -1;
    }
    
    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket()");
        return -1;
    }
    
    if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("setting O_NONBLOCK");
        close(sfd);
        return -1;
    }
    
    /*
     * Clean up a previous socket file if we left it around
     */
    if (lstat(path, &tstat) == 0) {
        if (S_ISSOCK(tstat.st_mode))
            unlink(path);
    }

    flags = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
    setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
    setsockopt(sfd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));

    /*
     * the memset call clears nonstandard fields in some impementations
     * that otherwise mess things up.
     */
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    old_umask=umask( ~(access_mask&0777));
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind()");
        close(sfd);
        umask(old_umask);
        return -1;
    }
    umask(old_umask);
    if (listen(sfd, EBB_MAX_CLIENTS) == -1) {
        perror("listen()");
        close(sfd);
        return -1;
    }
    return sfd;
}