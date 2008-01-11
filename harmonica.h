/* Harmonica Web Server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * All rights reserved.
 */

#include <glib.h>
#include "tcp_server.h"
#include "mongrel/parser.h"

#ifndef harmonica_h
#define harmonica_h

typedef void (*harmonica_request_cb_t) (harmonica*, tcp_client *client, GHash *env);

typedef struct harmonica {
  struct http_parser parser;
  tcp_server server;
  harmonica_request_cb_t request_cb;
} harmonica;

harmonica* harmonica_new(harmonica_error_cb);
void harmonica_free(harmonica*);
void harmonica_stop(harmonica*);
void harmonica_start(harmonica*, char *host, int port, harmonica_request_cb_t);

#endif harmonica_h