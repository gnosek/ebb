/* Is there a way to get the preprocessor to do this so that this file isn't
 * so redundant? */

void ebb_http_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(field, flen, value, vlen);
  g_queue_push_head(client->env, pair);
}

void ebb_request_method_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_METHOD";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}

void ebb_request_uri_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_URI";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}

void ebb_fragment_cb(void *data, const char *at, size_t length)
{
  const char *key = "FRAGMENT";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}

void ebb_request_path_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_PATH";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}

void ebb_query_string_cb(void *data, const char *at, size_t length)
{
  const char *key = "QUERY_STRING";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}

void ebb_http_version_cb(void *data, const char *at, size_t length)
{
  const char *key = "HTTP_VERSION";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}

/* TODO: this isn't handled correctly. Browser might send
 * Expect: 100-continue
 * Should ebb handle that?
 */
void ebb_header_done_cb(void *data, const char *at, size_t length)
{
  const char *key = "body";
  ebb_client *client = (ebb_client*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(client->env, pair);
}