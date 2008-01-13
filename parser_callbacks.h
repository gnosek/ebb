/* Is there a way to get the preprocessor to do this so that this file isn't
 * so redundant? */

void ebb_http_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(field, flen, value, vlen);
  g_queue_push_head(request->env, pair);
}

void ebb_request_method_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_METHOD";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void ebb_request_uri_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_URI";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void ebb_fragment_cb(void *data, const char *at, size_t length)
{
  const char *key = "FRAGMENT";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void ebb_request_path_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_PATH";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void ebb_query_string_cb(void *data, const char *at, size_t length)
{
  const char *key = "QUERY_STRING";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void ebb_http_version_cb(void *data, const char *at, size_t length)
{
  const char *key = "HTTP_VERSION";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

/* TODO: this isn't handled correctly. Browser might send
 * Expect: 100-continue
 * Should ebb handle that?
 */
void ebb_header_done_cb(void *data, const char *at, size_t length)
{
  const char *key = "body";
  ebb_request *request = (ebb_request*)(data);
  ebb_env_pair *pair = ebb_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}