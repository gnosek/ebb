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

/*** Drum Server ***/

typedef void (*drum_request_cb_t)( drum_server *    
                                 , tcp_client *client
                                 , GHashTable *env
                                 );

drum_server* drum_server_new(error_cb_t);
void drum_server_free(drum_server*);
void drum_server_stop(drum_server*);
void drum_server_start(drum_server*, char *host, int port, drum_request_cb_t);

struct drum_server {
  tcp_server *tcp_server;
  drum_request_cb_t request_cb;
};

/*** Drum Request ***/

drum_request* drum_request_new(drum_server *, tcp_client *);
void drum_request_free(drum_request *);
void drum_request_read_cb(tcp_client*, char *buffer, int length, void *data);

struct drum_request {
  drum_server *server;
  tcp_client *client;
  http_parser *parser;
  GHashTable *env;
  GString *buffer;
};


#endif drum_h