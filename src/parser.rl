/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */
#include "parser.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define LEN(AT, FPC) (FPC - buffer - parser->AT)
#define MARK(M,FPC) (parser->M = (FPC) - buffer)
#define PTR_TO(F) (buffer + parser->F)

/** machine **/
%%{
  machine http_parser;
  
  action mark {MARK(mark, fpc); }
  
  action start_field { MARK(field_start, fpc); }
  action write_field { 
    parser->field_len = LEN(field_start, fpc);
    if(parser->field_len > 256) {
      parser->overflow_error = TRUE;
      fbreak;
    }
  }
  
  action start_value { MARK(mark, fpc); }
  action write_value {
    if(LEN(mark, fpc) > 80 * 1024) { parser->overflow_error = TRUE; fbreak; }
    if(parser->http_field != NULL) {
      parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, fpc));
    }
  }
  
  action http_accept {
    if(LEN(mark, fpc) > 1024) { parser->overflow_error = TRUE; fbreak; }
    parser->on_element(parser->data, MONGREL_ACCEPT, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action http_content_length {
    if(LEN(mark, fpc) > 20) { parser->overflow_error = TRUE; fbreak; }
    set_content_length(parser, PTR_TO(mark), LEN(mark, fpc));
    parser->on_element(parser->data, MONGREL_CONTENT_LENGTH, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action http_content_type {
    if(LEN(mark, fpc) > 1024) { parser->overflow_error = TRUE; fbreak; }
    parser->on_element(parser->data, MONGREL_CONTENT_TYPE, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action fragment {
    /* Don't know if this length is specified somewhere or not */
    if(LEN(mark, fpc) > 1024) { parser->overflow_error = TRUE; fbreak; }
    parser->on_element(parser->data, MONGREL_FRAGMENT, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action http_version {
    parser->on_element(parser->data, MONGREL_HTTP_VERSION, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action request_path {
    if(LEN(mark, fpc) > 1024) {
      parser->overflow_error = TRUE;
      fbreak;
    }
    parser->on_element(parser->data, MONGREL_REQUEST_PATH, PTR_TO(mark), LEN(mark,fpc));
  }
  
  action request_method { 
    parser->on_element(parser->data, MONGREL_REQUEST_METHOD, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action request_uri {
    if(LEN(mark, fpc) > 12 * 1024) {
      parser->overflow_error = TRUE;
      fbreak;
    }
    parser->on_element(parser->data, MONGREL_REQUEST_URI, PTR_TO(mark), LEN(mark, fpc));
  }
  
  action start_query {MARK(query_start, fpc); }
  action query_string { 
    if(LEN(query_start, fpc) > 10 * 1024) {
      parser->overflow_error = TRUE;
      fbreak;
    }
    parser->on_element(parser->data, MONGREL_QUERY_STRING, PTR_TO(query_start), LEN(query_start, fpc));
  }
  
  
  action done {
    if(parser->nread > 1024 * (80 + 32)) {
      parser->overflow_error = TRUE;
      fbreak;
    }
    parser->body_start = fpc - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, fpc + 1, pe - fpc - 1);
    fbreak;
  }

#### HTTP PROTOCOL GRAMMAR
# line endings
  CRLF = "\r\n";

# character types
  CTL = (cntrl | 127);
  safe = ("$" | "-" | "_" | ".");
  extra = ("!" | "*" | "'" | "(" | ")" | ",");
  reserved = (";" | "/" | "?" | ":" | "@" | "&" | "=" | "+");
  unsafe = (CTL | " " | "\"" | "#" | "%" | "<" | ">");
  national = any -- (alpha | digit | reserved | extra | safe | unsafe);
  unreserved = (alpha | digit | safe | extra | national);
  escape = ("%" xdigit xdigit);
  uchar = (unreserved | escape);
  pchar = (uchar | ":" | "@" | "&" | "=" | "+");
  tspecials = ("(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\\" | "\"" | "/" | "[" | "]" | "?" | "=" | "{" | "}" | " " | "\t");

# elements
  token = (ascii -- (CTL | tspecials));

# URI schemes and absolute paths
  scheme = ( alpha | digit | "+" | "-" | "." )* ;
  absolute_uri = (scheme ":" (uchar | reserved )*);

  path = ( pchar+ ( "/" pchar* )* ) ;
  query = ( uchar | reserved )* %query_string ;
  param = ( pchar | "/" )* ;
  params = ( param ( ";" param )* ) ;
  rel_path = ( path? %request_path (";" params)? ) ("?" %start_query query)?;
  absolute_path = ( "/"+ rel_path );

  Request_URI = ( "*" | absolute_uri | absolute_path ) >mark %request_uri;
  Fragment = ( uchar | reserved )* >mark %fragment;
  Method = ( upper | digit | safe ){1,20} >mark %request_method;

  http_number = ( digit+ "." digit+ ) ;
  HTTP_Version = ( "HTTP/" http_number ) >mark %http_version ;
  Request_Line = ( Method " " Request_URI ("#" Fragment){0,1} " " HTTP_Version CRLF ) ;

  field_name = ( token -- ":" )+ >start_field %write_field;

  field_value = any* >start_value %write_value;
  
  known_header = ( ("Accept:"i         " "* (any* >mark %http_accept))
                 | ("Content-Length:"i " "* (digit+ >mark %http_content_length))
                 | ("Content-Type:"i   " "* (any* >mark %http_content_type))
                 ) :> CRLF;
  unknown_header = (field_name ":" " "* field_value :> CRLF) -- known_header;
  
  Request = Request_Line (known_header | unknown_header)* ( CRLF @done );

main := Request;

}%%

/** Data **/
%% write data;

static void set_content_length(http_parser *parser, const char *at, int length)
{
  /* atoi_length - why isn't this in the statndard library? i hate c */
  assert(parser->content_length == 0);
  int i, mult;
  for(mult=1, i=length-1; i>=0; i--, mult*=10)
    parser->content_length += (at[i] - '0') * mult;
}

void http_parser_init(http_parser *parser)  {
  int cs = 0;
  %% write init;
  parser->cs = cs;
  parser->overflow_error = FALSE;
  parser->body_start = 0;
  parser->content_length = 0;
  parser->mark = 0;
  parser->nread = 0;
  parser->field_len = 0;
  parser->field_start = 0;
  parser->data = NULL;
  parser->http_field = NULL;
}


/** exec **/
size_t http_parser_execute(http_parser *parser, const char *buffer, size_t len, size_t off)  {
  const char *p, *pe;
  int cs = parser->cs;
  
  assert(off <= len && "offset past end of buffer");
  
  p = buffer+off;
  pe = buffer+len;
  
  /* Ragel 6 does not require this */
  // assert(*pe == '\0' && "pointer does not end on NUL");
  assert(pe - p == len - off && "pointers aren't same distance");
  
  %% write exec;
  
  parser->cs = cs;
  parser->nread += p - (buffer + off);
  
  assert(p <= pe && "buffer overflow after parsing execute");
  assert(parser->nread <= len && "nread longer than length");
  assert(parser->body_start <= len && "body starts after buffer end");
  assert(parser->mark < len && "mark is after buffer end");
  assert(parser->field_len <= len && "field has length longer than whole buffer");
  assert(parser->field_start < len && "field starts after buffer end");
  
  /* Ragel 6 does not use write eof; no need for this
  if(parser->body_start) {
    // final \r\n combo encountered so stop right here 
    parser->nread++;
    %% write eof;
  }
  */
  
  return(parser->nread);
}

int http_parser_finish(http_parser *parser)
{
  if (http_parser_has_error(parser))
    return -1;
  else if (http_parser_is_finished(parser))
    return 1;
  else
    return 0;
}

int http_parser_has_error(http_parser *parser) {
  return parser->cs == http_parser_error || parser->overflow_error;
}

int http_parser_is_finished(http_parser *parser) {
  return parser->cs >= http_parser_first_final;
}
