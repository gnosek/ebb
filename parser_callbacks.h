/* Is there a way to get the preprocessor to do this so that this file isn't
 * so redundant?
 */

void drum_http_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen)
{
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(field, flen, value, vlen);
  g_queue_push_head(request->env, pair);
}

void drum_request_method_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_METHOD";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void drum_request_uri_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_URI";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void drum_fragment_cb(void *data, const char *at, size_t length)
{
  const char *key = "FRAGMENT";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void drum_request_path_cb(void *data, const char *at, size_t length)
{
  const char *key = "REQUEST_PATH";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void drum_query_string_cb(void *data, const char *at, size_t length)
{
  const char *key = "QUERY_STRING";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void drum_http_version_cb(void *data, const char *at, size_t length)
{
  const char *key = "HTTP_VERSION";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}

void drum_header_done_cb(void *data, const char *at, size_t length)
{
  const char *key = "body";
  drum_request *request = (drum_request*)(data);
  drum_env_pair *pair = drum_env_pair_new(key, strlen(key), at, length);
  g_queue_push_head(request->env, pair);
}