/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */

#ifndef http11_parser_h
#define http11_parser_h

#include <sys/types.h>

#if defined(_WIN32)
#include <stddef.h>
#endif


enum { MONGREL_REQUEST_METHOD
     , MONGREL_REQUEST_URI
     , MONGREL_FRAGMENT
     , MONGREL_REQUEST_PATH
     , MONGREL_QUERY_STRING
     , MONGREL_HTTP_VERSION
     , MONGREL_CONTENT_LENGTH
     , MONGREL_CONTENT_TYPE
     /* below - not used in the parser but often used by users of parser */
     , MONGREL_SERVER_PORT 
     };

typedef void (*element_cb)(void *data, const char *at, size_t length);
typedef void (*field_cb)(void *data, const char *field, size_t flen, const char *value, size_t vlen);
typedef void (*new_element_cb)(void *data, int type, const char *at, size_t length);



typedef struct http_parser { 
  int cs;
  int overflow_error;
  size_t body_start;
  size_t content_length;
  size_t nread;
  size_t mark;
  size_t field_start;
  size_t field_len;
  size_t query_start;

  void *data;

  field_cb http_field;
  
  element_cb header_done;
  new_element_cb on_element;
} http_parser;

void http_parser_init(http_parser *parser);
int http_parser_finish(http_parser *parser);
size_t http_parser_execute(http_parser *parser, const char *data, size_t len, size_t off);
int http_parser_has_error(http_parser *parser);
int http_parser_is_finished(http_parser *parser);

#define http_parser_nread(parser) (parser)->nread 

#endif
