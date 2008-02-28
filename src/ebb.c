/* Ebb Web Server
 * Copyright (c) 2007 Ry Dahl
 * This software is released under the "MIT License". See README file for details.
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

static int server_socket(const int port);
static int server_socket_unix(const char *path, int access_mask);

#define env_add(client, field,flen,value,vlen)                              \
  client->env_fields[client->env_size] = field;                             \
  client->env_field_lengths[client->env_size] = flen;                       \
  client->env_values[client->env_size] = value;                             \
  client->env_value_lengths[client->env_size] = vlen;                       \
  client->env_size += 1;                                                     
#define env_add_const(client,field,value,vlen)                              \
  client->env_fields[client->env_size] = NULL;                              \
  client->env_field_lengths[client->env_size] = field;                      \
  client->env_values[client->env_size] = value;                             \
  client->env_value_lengths[client->env_size] = vlen;                       \
  client->env_size += 1;                                                     
#define env_error(client)                                                   \
  client->env_fields[client->env_size] = NULL;                              \
  client->env_field_lengths[client->env_size] = -1;                         \
  client->env_values[client->env_size] = NULL;                              \
  client->env_value_lengths[client->env_size] = -1;                         \
  client->env_size += 1;                                                     

int env_has_error(ebb_client *client)
{
  int i;
  for(i = 0; i < client->env_size; i++)
    if(client->env_field_lengths[i] < 0)
      return TRUE;
  return FALSE;
}

/** Defines common length and error messages for input length validation. */
#define DEF_MAX_LENGTH(N,length) const size_t MAX_##N##_LENGTH = length; const char *MAX_##N##_LENGTH_ERR = "HTTP Parse Error: HTTP element " # N  " is longer than the " # length " allowed length."

/** Validates the max length of given input and throws an exception if over. */
#define VALIDATE_MAX_LENGTH(len, N) if(len > MAX_##N##_LENGTH) { env_error(client); g_message(MAX_##N##_LENGTH_ERR); }

/* Defines the maximum allowed lengths for various input elements.*/
DEF_MAX_LENGTH(FIELD_NAME, 256);
DEF_MAX_LENGTH(FIELD_VALUE, 80 * 1024);
DEF_MAX_LENGTH(REQUEST_URI, 1024 * 12);
DEF_MAX_LENGTH(FRAGMENT, 1024); /* Don't know if this length is specified somewhere or not */
DEF_MAX_LENGTH(REQUEST_PATH, 1024);
DEF_MAX_LENGTH(QUERY_STRING, (1024 * 10));
DEF_MAX_LENGTH(HEADER, (1024 * (80 + 32)));

void http_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(flen, FIELD_NAME);
  VALIDATE_MAX_LENGTH(vlen, FIELD_VALUE);
  env_add(client, field, flen, value, vlen);
}


void request_method_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  env_add_const(client, EBB_REQUEST_METHOD, at, length);
}


void request_uri_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, REQUEST_URI);
  env_add_const(client, EBB_REQUEST_URI, at, length);
}


void fragment_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, FRAGMENT);
  env_add_const(client, EBB_FRAGMENT, at, length);
}


void request_path_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, REQUEST_PATH);
  env_add_const(client, EBB_REQUEST_PATH, at, length);
}


void query_string_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, QUERY_STRING);
  env_add_const(client, EBB_QUERY_STRING, at, length);
}


void http_version_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  env_add_const(client, EBB_HTTP_VERSION, at, length);
}


void content_length_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  env_add_const(client, EBB_CONTENT_LENGTH, at, length);
  
  /* i hate c. */
  char buf[20];
  strncpy(buf, at, length);
  buf[length] = '\0';
  client->content_length = atoi(buf);
}


const char* localhost_str = "0.0.0.0";
void dispatch(ebb_client *client)
{
  ebb_server *server = client->server;
  
  if(client->open == FALSE)
    return;
  
  /* Set the env variables */
  if(server->port) {
    env_add_const(client, EBB_SERVER_NAME
                        , localhost_str
                        , 7
                        );
    env_add_const(client, EBB_SERVER_PORT
                        , server->port
                        , strlen(server->port)
                        );
  }
  server->request_cb(client, server->request_cb_data);
}


void on_timeout(struct ev_loop *loop, ev_timer *watcher, int revents)
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
#define total_request_size (client->content_length + client->parser.nread)

void* read_body_into_file(void *_client)
{
  ebb_client *client = (ebb_client*)_client;
  static unsigned int id;
  FILE *tmpfile;
  
  assert(client->open);
  assert(client->server->open);
  assert(client->content_length > 0);
  assert(client_finished_parsing);
  
  /* set blocking socket */
  int flags = fcntl(client->fd, F_GETFL, 0);
  assert(0 <= fcntl(client->fd, F_SETFL, flags & ~O_NONBLOCK));
  
  sprintf(client->upload_file_filename, "/tmp/ebb_upload_%010d", id++);
  tmpfile = fopen(client->upload_file_filename, "w+");
  if(tmpfile == NULL) g_message("Cannot open tmpfile %s", client->upload_file_filename);
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
  
  // g_debug("wrote request header to file. written: %d, content_length: %d", written, client->content_length);
  
  int bufsize = 5*1024;
  char buffer[bufsize];
  size_t received;
  while(written < client->content_length) {
    received = recv(client->fd
                   , buffer
                   , min(client->content_length - written, bufsize)
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
  // g_debug("%d bytes written to file %s", written, client->upload_file_filename);
  return NULL;
error:
  ebb_client_close(client);
  return NULL;
}


void on_readable(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_client *client = (ebb_client*)(watcher->data);
  
  assert(client->open);
  assert(client->server->open);
  assert(client->server->loop == loop);
  assert(&client->read_watcher == watcher);
  
  ssize_t read = recv( client->fd
                     , client->request_buffer + client->read
                     , EBB_BUFFERSIZE - client->read - 1 /* -1 is for making ragel happy below */
                     , 0
                     );
  if(read <= 0) goto error; /* XXX is this the right action to take for read==0 ? */
  client->read += read;
  ev_timer_again(loop, &client->timeout_watcher);
  
  if(FALSE == client_finished_parsing) {
    client->request_buffer[client->read] = '\0'; /* make ragel happy */
    http_parser_execute( &client->parser
                       , client->request_buffer
                       , client->read
                       , client->parser.nread
                       );
    if(http_parser_has_error(&client->parser)) goto error;
  }
  
  if(total_request_size == client->read) {
    ev_io_stop(loop, watcher);
    client->nread_from_body = 0;
    dispatch(client);
    return;
  }
  
  if(client_finished_parsing && total_request_size > EBB_BUFFERSIZE ) {
    /* read body into file - in a thread */
    pthread_t thread;
    ev_io_stop(loop, watcher);
    assert(0 <= pthread_create(&thread, NULL, read_body_into_file, client));
    pthread_join(thread, NULL);
    dispatch(client);
    return;
  }
  return;
error:
  if(read < 0) g_message("Error recving data: %s", strerror(errno));
  ebb_client_close(client);
}

void on_request(struct ev_loop *loop, ev_io *watcher, int revents)
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
    if(!server->clients[i].open) {
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
  client->open = TRUE;
  client->server = server;
  
  /* DO SOCKET STUFF */
  socklen_t len;
  client->fd = accept(server->fd, (struct sockaddr*)&(server->sockaddr), &len);
  assert(client->fd >= 0);
  int flags = fcntl(client->fd, F_GETFL, 0);
  assert(0 <= fcntl(client->fd, F_SETFL, flags | O_NONBLOCK));
  
  /* INITIALIZE http_parser */
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
  
  /* OTHER */
  client->env_size = 0;
  client->read = client->nread_from_body = 0;
  client->response_buffer->len = 0; /* see note in ebb_client_close */
  client->content_length = 0;
  
  /* SETUP READ AND TIMEOUT WATCHERS */
  client->read_watcher.data = client;
  ev_init(&client->read_watcher, on_readable);
  ev_io_set(&client->read_watcher, client->fd, EV_READ | EV_ERROR);
  ev_io_start(server->loop, &client->read_watcher);
  
  client->timeout_watcher.data = client;  
  ev_timer_init(&client->timeout_watcher, on_timeout, EBB_TIMEOUT, EBB_TIMEOUT);
  ev_timer_start(server->loop, &client->timeout_watcher);
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
  for(i=0; i < EBB_MAX_CLIENTS; i++)
    server->clients[i].response_buffer = g_string_new("");
  
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
    //g_message("Stopping Ebb server");
    int i;
    ebb_client *client;
    //for(i=0; i < EBB_MAX_CLIENTS; i++)
    //  ebb_client_close(client);
    ev_io_stop(server->loop, &server->request_watcher);
    close(server->fd);
    if(server->socketpath)
      unlink(server->socketpath);
    server->open = FALSE;
  }
}


void ebb_server_listen(ebb_server *server)
{
  int r = listen(server->fd, EBB_MAX_CLIENTS);
  assert(r >= 0);
  assert(server->open == FALSE);
  server->open = TRUE;
  
  server->request_watcher.data = server;
  ev_init (&server->request_watcher, on_request);
  ev_io_set (&server->request_watcher, server->fd, EV_READ | EV_ERROR);
  ev_io_start (server->loop, &server->request_watcher);
}


int ebb_server_listen_on_port(ebb_server *server, const int port)
{
  int fd = server_socket(port);
  if(fd < 0) return 0;
  server->port = malloc(sizeof(char)*8); /* for easy access to the port */
  sprintf(server->port, "%d", port);
  server->fd = fd;
  ebb_server_listen(server);
  return fd;
}


int ebb_server_listen_on_socket(ebb_server *server, const char *socketpath)
{
  int fd = server_socket_unix(socketpath, 0755);
  if(fd < 0) return 0;
  server->socketpath = strdup(socketpath);
  server->fd = fd;
  ebb_server_listen(server);
  return fd;
}


void ebb_client_close(ebb_client *client)
{
  if(client->open) {
    ev_io_stop(client->server->loop, &client->read_watcher);
    ev_io_stop(client->server->loop, &client->write_watcher);
    ev_timer_stop(client->server->loop, &client->timeout_watcher);
    
    if(client->upload_file) {
      fclose(client->upload_file);
      unlink(client->upload_file_filename);
    }
    
    /* here we do not free the already allocated GString client->response_buffer
     * that we're holding the response in. we reuse it again - 
     * presumably because the backend is going to keep sending such long
     * requests.
     */
    client->response_buffer->len = 0;
    
    close(client->fd);
    client->open = FALSE;
  }
}


void on_client_writable(struct ev_loop *loop, ev_io *watcher, int revents)
{
  ebb_client *client = (ebb_client*)(watcher->data);
  ssize_t sent;
  
  if(EV_ERROR & revents) {
    g_message("on_client_writable() got error event, closing peer");
    return;
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
    ebb_client_close(client);
    return;
  }
  client->written += sent;
  
  assert(client->written <= client->response_buffer->len);
  //g_message("wrote %d bytes. total: %d", (int)sent, (int)(client->written));
  
  ev_timer_again(loop, &(client->timeout_watcher));
  
  if(client->written == client->response_buffer->len)
    ebb_client_close(client);
}


void ebb_client_write(ebb_client *client, const char *data, int length)
{
  g_string_append_len(client->response_buffer, data, length);
}


void ebb_client_finished(ebb_client *client)
{
  assert(client->open);
  assert(FALSE == ev_is_active(&(client->write_watcher)));
  
  /* assure the socket is still in non-blocking mode
   * in the ruby binding, for example, i change this flag 
   */
  int flags = fcntl(client->fd, F_GETFL, 0);
  if(0 > fcntl(client->fd, F_SETFL, flags | O_NONBLOCK)) {
    perror("fcntl()");
    ebb_client_close(client);
    return;
  }
  
  client->written = 0;
  client->write_watcher.data = client;
  ev_init (&(client->write_watcher), on_client_writable);
  ev_io_set (&(client->write_watcher), client->fd, EV_WRITE | EV_ERROR);
  ev_io_start(client->server->loop, &(client->write_watcher));
}


/* pass an allocated buffer and the length to read. this function will try to
 * fill the buffer with that length of data read from the body of the request.
 * the return value says how much was actually written.
 */
int ebb_client_read(ebb_client *client, char *buffer, int length)
{
  size_t read;
  
  assert(client->open);
  assert(client_finished_parsing);
  
  if(client->upload_file) {
    read = fread(buffer, 1, length, client->upload_file);
    /* TODO error checking! */
    return read;
  } else {
    char* request_body = client->request_buffer + client->parser.nread;
    
    read = ramp(min(length, client->content_length - client->nread_from_body));
    memcpy( buffer
          , request_body + client->nread_from_body
          , read
          );
    client->nread_from_body += read;
    return read;
  }
}

/* The following socket creation routines are modified and stolen from memcached */

static int server_socket(const int port) {
    int sfd;
    struct linger ling = {0, 0};
    struct sockaddr_in addr;
    int flags =1;
    
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket()");
        return -1;
    }
    
    if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("setting O_NONBLOCK");
        close(sfd);
        return -1;
    }
    
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
    
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind()");
        close(sfd);
        return -1;
    }
    if (listen(sfd, EBB_MAX_CLIENTS) == -1) {
        perror("listen()");
        close(sfd);
        return -1;
    }
    return sfd;
}


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

