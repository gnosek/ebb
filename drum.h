/* Drum Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#include <glib.h>
#include "tcp_server.h"
#include "mongrel/parser.h"

#ifndef drum_h
#define drum_h

typedef struct drum_server drum_server;
typedef struct drum_request drum_request;

#define DRUM_LOG_DOMAIN "Drum"
#define drum_error(str, ...)  \
  g_log(DRUM_LOG_DOMAIN, G_LOG_LEVEL_ERROR, str, ## __VA_ARGS__);


/*** Drum Server ***/

typedef void (*drum_request_cb_t)(drum_request*, void*);

drum_server* drum_server_new();
void drum_server_free(drum_server*);
void drum_server_stop(drum_server*);
void drum_server_start(drum_server*, char *host, int port, drum_request_cb_t, void *request_cb_data);

struct drum_server {
  tcp_server *tcp_server;
  void *request_cb_data;
  drum_request_cb_t request_cb;
};

/*** Drum Request ***/

drum_request* drum_request_new(drum_server *, tcp_client *);
void drum_request_free(drum_request *);

struct drum_request {
  drum_server *server;
  tcp_client *client;
  http_parser *parser;
  GQueue *env; /* queue of drum_env_pairs */
  GString *buffer;
};

typedef struct drum_env_pair {
  char *field;
  size_t flen;
  
  char *value;
  size_t vlen;
} drum_env_pair;

drum_env_pair* drum_env_pair_new(const char *field, size_t flen, const char *value, size_t vlen);
#define drum_env_pair_free(pair) free(pair)

#endif drum_h