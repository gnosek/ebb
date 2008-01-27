/* Is there a way to get the preprocessor to do this so that this file isn't
 * so redundant? */
#ifndef parser_callbacks_h
#define parser_callbacks_h

#include "ebb.h"

void http_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env(client, field, flen, value, vlen);
}

void request_method_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_REQUEST_METHOD, at, length);
}

void request_uri_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_REQUEST_URI, at, length);
}

void fragment_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_FRAGMENT, at, length);
}

void request_path_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_REQUEST_PATH, at, length);
}

void query_string_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_QUERY_STRING, at, length);
}

void http_version_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_HTTP_VERSION, at, length);
}

void header_done_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_client_add_env_const(client, EBB_REQUEST_BODY, at, length);
}

#endif parser_callbacks_h