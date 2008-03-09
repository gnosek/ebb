/* A ruby binding to the ebb web server
 * Copyright (c) 2008 Ry Dahl. This software is released under the MIT 
 * License. See README file for details.
 */
#include <ruby.h>
#include <assert.h>
#include <fcntl.h>
#include <ebb.h>
#include <ev.h>
#include <pthread.h>
#include <glib.h>

static VALUE cClient;
static VALUE global_http_prefix;
static VALUE global_request_method;
static VALUE global_request_uri;
static VALUE global_fragment;
static VALUE global_request_path;
static VALUE global_query_string;
static VALUE global_http_version;
static VALUE global_request_body;
static VALUE global_server_name;
static VALUE global_server_port;
static VALUE global_path_info;
static VALUE global_content_length;
static VALUE global_http_host;

/* You don't want to run more than one server per Ruby VM. Really
 * I'm making this explicit by not defining a Ebb::Server class but instead
 * initializing a single server and single event loop on module load.
 */
static ebb_server *server;
struct ev_loop *loop;
static notify_fd;
static pthread_mutex_t waiting_clients_lock = PTHREAD_MUTEX_INITIALIZER;
static GQueue *waiting_clients;


/* Variables with a leading underscore are C-level variables */

#define ASCII_UPPER(ch) ('a' <= ch && ch <= 'z' ? ch - 'a' + 'A' : ch)
#ifndef RSTRING_PTR
# define RSTRING_PTR(s) (RSTRING(s)->ptr)
# define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

void request_cb(ebb_client *client, void *_)
{
  pthread_mutex_lock(&waiting_clients_lock);
  g_queue_push_tail(waiting_clients, (void*)client);
  pthread_mutex_unlock(&waiting_clients_lock);
  
  assert(notify_fd > 2);
  assert(1 == write(notify_fd, "N", 1));
}

static void
oneshot_timeout (struct ev_loop *loop, struct ev_timer *w, int revents) {;}

static void* process_connections(void *_)
{
  ev_loop(loop, 0);
}

VALUE server_next_client(VALUE _)
{
  ebb_client *client;
  VALUE rb_client;
  
  pthread_mutex_lock(&waiting_clients_lock);
  client = g_queue_pop_head(waiting_clients);
  pthread_mutex_unlock(&waiting_clients_lock);
  
  if(client == NULL)
    rb_client = Qnil;
  else
    rb_client = Data_Wrap_Struct(cClient, 0, 0, client);
  return rb_client;
}

VALUE server_listen_on_port(VALUE _, VALUE port)
{
  if(ebb_server_listen_on_port(server, FIX2INT(port)) < 0)
    rb_sys_fail("Problem listening on port");
  pthread_t thread;
  int fildes[2];
  assert(0 <= pipe(fildes));
  
  /* the read side is blocking */
  int flags = fcntl(fildes[0], F_GETFL, 0);
  assert(0 <= fcntl(fildes[0], F_SETFL, flags & ~O_NONBLOCK));
  /* the write side is nonblocking */
  flags = fcntl(fildes[1], F_GETFL, 0);
  assert(0 <= fcntl(fildes[1], F_SETFL, flags | O_NONBLOCK));
  notify_fd = fildes[1];
  
  assert(0 <= pthread_create(&thread, NULL, process_connections, NULL));
  pthread_detach(thread);
  
  return INT2FIX(fildes[0]);
}


VALUE server_unlisten(VALUE _)
{
  ebb_server_unlisten(server);
  return Qnil;
}


VALUE env_field(struct ebb_env_item *item)
{
  VALUE f;
  switch(item->type) {
    case EBB_FIELD_VALUE_PAIR:
      f = rb_str_new(NULL, RSTRING_LEN(global_http_prefix) + item->field_length);
      memcpy( RSTRING_PTR(f)
            , RSTRING_PTR(global_http_prefix)
            , RSTRING_LEN(global_http_prefix)
            );
      int i;
      for(i = 0; i < item->field_length; i++) {
        char *ch = RSTRING_PTR(f) + RSTRING_LEN(global_http_prefix) + i;
        *ch = item->field[i] == '-' ? '_' : ASCII_UPPER(item->field[i]);
      }
      return f;
    case EBB_REQUEST_METHOD:  return global_request_method;
    case EBB_REQUEST_URI:     return global_request_uri;
    case EBB_FRAGMENT:        return global_fragment;
    case EBB_REQUEST_PATH:    return global_request_path;
    case EBB_QUERY_STRING:    return global_query_string;
    case EBB_HTTP_VERSION:    return global_http_version;
    case EBB_SERVER_NAME:     return global_server_name;
    case EBB_SERVER_PORT:     return global_server_port;
    case EBB_CONTENT_LENGTH:  return global_content_length;
  }
  assert(FALSE);
  return Qnil;
}


VALUE env_value(struct ebb_env_item *item)
{
  if(item->value_length > 0)
    return rb_str_new(item->value, item->value_length);
  else
    return Qnil;
}


VALUE client_env(VALUE _, VALUE client)
{
  ebb_client *_client;
  VALUE hash = rb_hash_new();
  int i;  
  Data_Get_Struct(client, ebb_client, _client);
  
  for(i=0; i < _client->env_size; i++) {
    rb_hash_aset(hash, env_field(&_client->env[i])
                     , env_value(&_client->env[i])
                     );
  }
  rb_hash_aset(hash, global_path_info, rb_hash_aref(hash, global_request_path));
  return hash;
}


VALUE client_read_input(VALUE _, VALUE client, VALUE size)
{
  ebb_client *_client;
  GString *_string;
  VALUE string;
  int _size = FIX2INT(size);
  Data_Get_Struct(client, ebb_client, _client);
  
  string = rb_str_buf_new( _size );
  int nread = ebb_client_read(_client, RSTRING_PTR(string), _size);
#if RUBY_VERSION_CODE < 190
  RSTRING(string)->len = nread;
#else
  rb_str_set_len(string, nread);
#endif
  
  if(nread < 0)
    rb_raise(rb_eRuntimeError,"There was a problem reading from input (bad tmp file?)");
  if(nread == 0)
    return Qnil;
  return string;
}

VALUE client_write_status(VALUE _, VALUE client, VALUE status, VALUE human_status)
{
  ebb_client *_client;
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_write_status(_client, FIX2INT(status), StringValuePtr(human_status));
  return Qnil;
}

VALUE client_write_header(VALUE _, VALUE client, VALUE field, VALUE value)
{
  ebb_client *_client;
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_write_header(_client, StringValuePtr(field), StringValuePtr(value));
  return Qnil;
}

VALUE client_write(VALUE _, VALUE client, VALUE string)
{
  ebb_client *_client;
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_write(_client, RSTRING_PTR(string), RSTRING_LEN(string));
  return Qnil;
}


VALUE client_finished(VALUE _, VALUE client)
{
  ebb_client *_client;
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_finished(_client);
  return Qnil;
}


void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  VALUE mFFI = rb_define_module_under(mEbb, "FFI");
  
  /** Defines global strings in the init method. */
#define DEF_GLOBAL(N, val) global_##N = rb_obj_freeze(rb_str_new2(val)); rb_global_variable(&global_##N)
  DEF_GLOBAL(http_prefix, "HTTP_");
  DEF_GLOBAL(request_method, "REQUEST_METHOD");  
  DEF_GLOBAL(request_uri, "REQUEST_URI");
  DEF_GLOBAL(fragment, "FRAGMENT");
  DEF_GLOBAL(request_path, "REQUEST_PATH");
  DEF_GLOBAL(query_string, "QUERY_STRING");
  DEF_GLOBAL(http_version, "HTTP_VERSION");
  DEF_GLOBAL(request_body, "REQUEST_BODY");
  DEF_GLOBAL(server_name, "SERVER_NAME");
  DEF_GLOBAL(server_port, "SERVER_PORT");
  DEF_GLOBAL(path_info, "PATH_INFO");
  DEF_GLOBAL(content_length, "CONTENT_LENGTH");
  DEF_GLOBAL(http_host, "HTTP_HOST");

  rb_define_singleton_method(mFFI, "server_next_client", server_next_client, 0);
  
  rb_define_singleton_method(mFFI, "server_listen_on_port", server_listen_on_port, 1);
  rb_define_singleton_method(mFFI, "server_unlisten", server_unlisten, 0);
  
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  rb_define_singleton_method(mFFI, "client_read_input", client_read_input, 2);
  rb_define_singleton_method(mFFI, "client_write_status", client_write_status, 3);
  rb_define_singleton_method(mFFI, "client_write_header", client_write_header, 3);
  rb_define_singleton_method(mFFI, "client_write", client_write, 2);
  rb_define_singleton_method(mFFI, "client_finished", client_finished, 1);
  rb_define_singleton_method(mFFI, "client_env", client_env, 1);
  
  /* initialize ebb_server */
  loop = ev_default_loop (0);
  server = ebb_server_alloc();
  
  waiting_clients = g_queue_new();
  
  ebb_server_init(server, loop, request_cb, NULL);
}
