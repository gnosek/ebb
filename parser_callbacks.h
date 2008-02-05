/* Is there a way to get the preprocessor to do this so that this file isn't
 * so redundant? */
#ifndef parser_callbacks_h
#define parser_callbacks_h

#include "ebb.h"

/** Defines common length and error messages for input length validation. */
#define DEF_MAX_LENGTH(N,length) const size_t MAX_##N##_LENGTH = length; const char *MAX_##N##_LENGTH_ERR = "HTTP Parse Error: HTTP element " # N  " is longer than the " # length " allowed length."

/** Validates the max length of given input and throws an exception if over. */
#define VALIDATE_MAX_LENGTH(len, N) if(len > MAX_##N##_LENGTH) { ebb_env_error(client); ebb_info(MAX_##N##_LENGTH_ERR); }

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
  ebb_env_add(client, field, flen, value, vlen);
}

void request_method_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_env_add_const(client, EBB_REQUEST_METHOD, at, length);
}

void request_uri_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, REQUEST_URI);
  ebb_env_add_const(client, EBB_REQUEST_URI, at, length);
}

void fragment_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, FRAGMENT);
  ebb_env_add_const(client, EBB_FRAGMENT, at, length);
}

void request_path_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, REQUEST_PATH);
  ebb_env_add_const(client, EBB_REQUEST_PATH, at, length);
}

void query_string_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  VALIDATE_MAX_LENGTH(length, QUERY_STRING);
  ebb_env_add_const(client, EBB_QUERY_STRING, at, length);
}

void http_version_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_env_add_const(client, EBB_HTTP_VERSION, at, length);
}

void content_length_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_env_add_const(client, EBB_CONTENT_LENGTH, at, length);
  
  /* i hate c. */
  char buffer[30];
  strncpy(buffer, at, length);
  buffer[length] = '\0';
  client->content_length = atoi(buffer);
}

void header_done_cb(void *data, const char *at, size_t length)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_env_add_const(client, EBB_REQUEST_BODY, at, length);
}

#endif parser_callbacks_h