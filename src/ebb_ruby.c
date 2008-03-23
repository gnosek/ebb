/* A ruby binding to the ebb web server
 * Copyright (c) 2008 Ry Dahl. This software is released under the MIT 
 * License. See README file for details.
 */
#include <ruby.h>
#include <rubyio.h>
#include <rubysig.h>
#include <assert.h>
#include <fcntl.h>
#include <ebb.h>
#include <ev.h>

static VALUE cClient;
static VALUE global_fragment;
static VALUE global_path_info;
static VALUE global_query_string;
static VALUE global_request_body;
static VALUE global_request_method;
static VALUE global_request_path;
static VALUE global_request_uri;
static VALUE global_server_port;
static VALUE global_http_accept;
static VALUE global_http_connection;
static VALUE global_http_content_length;
static VALUE global_http_content_type;
static VALUE global_http_content_type;
static VALUE global_http_prefix;
static VALUE global_http_version;


/* You don't want to run more than one server per Ruby VM. Really
 * I'm making this explicit by not defining a Ebb::Server class but instead
 * initializing a single server and single event loop on module load.
 */
static ebb_server *server;
struct ev_loop *loop;
struct ev_idle idle_watcher;

/* Variables with a leading underscore are C-level variables */

#define ASCII_UPPER(ch) ('a' <= ch && ch <= 'z' ? ch - 'a' + 'A' : ch)
#ifndef RSTRING_PTR
# define RSTRING_PTR(s) (RSTRING(s)->ptr)
# define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

static void attach_idle_watcher()
{
  if(!ev_is_active(&idle_watcher)) {
    ev_idle_start (loop, &idle_watcher);
  }
}


static void detach_idle_watcher()
{
  ev_idle_stop(loop, &idle_watcher);
}

static int clients_in_use_p()
{
  int i;
  for(i = 0; i < EBB_MAX_CLIENTS; i++)
    if(server->clients[i].in_use) return TRUE;
  return FALSE;
}

void request_cb(ebb_client *client, void *data)
{
  VALUE waiting_clients = (VALUE)data;
  VALUE rb_client = Data_Wrap_Struct(cClient, 0, 0, client);
  rb_ary_push(waiting_clients, rb_client);
  attach_idle_watcher();
}

VALUE server_listen_on_port(VALUE _, VALUE port)
{
  if(ebb_server_listen_on_port(server, FIX2INT(port)) < 0)
    rb_sys_fail("Problem listening on port");
  return Qnil;
}

static void
idle_cb (struct ev_loop *loop, struct ev_idle *w, int revents) {
  if(clients_in_use_p()) {
    rb_thread_schedule();
  } else if(!rb_thread_alone()) {
    /* if you have another long running thread running besides the ones used
     * for the webapp's requests you will run into performance problems in 
     * ruby 1.8.x because rb_thread_select is slow. 
     * (Don't worry - you're probably not doing this.)
     */
    struct timeval select_timeout = { tv_sec: 0, tv_usec: 50000 };
    fd_set server_fd_set;
    FD_ZERO(&server_fd_set);
    FD_SET(server->fd, &server_fd_set);
    rb_thread_select(server->fd+1, &server_fd_set, 0, 0, &select_timeout);
  } else {
    detach_idle_watcher();
  }
}

VALUE server_process_connections(VALUE _)
{
  TRAP_BEG;
  ev_loop(loop, EVLOOP_ONESHOT);
  TRAP_END;
  return Qnil;
}


VALUE server_unlisten(VALUE _)
{
  ebb_server_unlisten(server);
  return Qnil;
}

VALUE server_open(VALUE _)
{
  return server->open ? Qtrue : Qfalse;
}

VALUE env_field(struct ebb_env_item *item)
{
  if(item->field) {
    VALUE f = rb_str_new(NULL, RSTRING_LEN(global_http_prefix) + item->field_length);
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
  }
  switch(item->type) {
    case MONGREL_ACCEPT:          return global_http_accept;
    case MONGREL_CONNECTION:      return global_http_connection;
    case MONGREL_CONTENT_LENGTH:  return global_http_content_length;
    case MONGREL_CONTENT_TYPE:    return global_http_content_type;
    case MONGREL_FRAGMENT:        return global_fragment;
    case MONGREL_HTTP_VERSION:    return global_http_version;
    case MONGREL_QUERY_STRING:    return global_query_string;
    case MONGREL_REQUEST_METHOD:  return global_request_method;
    case MONGREL_REQUEST_PATH:    return global_request_path;
    case MONGREL_REQUEST_URI:     return global_request_uri;
  }
  fprintf(stderr, "Unknown environ type: %d", item->type);
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


VALUE client_env(VALUE _, VALUE rb_client)
{
  ebb_client *client;
  VALUE field, value, env = rb_hash_new();
  int i;
  
  Data_Get_Struct(rb_client, ebb_client, client);
  for(i=0; i < client->env_size; i++) {
    field = env_field(&client->env[i]);
    value = env_value(&client->env[i]);
    rb_hash_aset(env, field, value);
  }
  
  if(client->server->port)
    rb_hash_aset(env, global_server_port, rb_str_new2(client->server->port));
  
  rb_hash_aset(env, global_path_info, rb_hash_aref(env, global_request_path));
  return env;
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

VALUE client_write_body(VALUE _, VALUE client, VALUE string)
{
  ebb_client *_client;
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_write_body(_client, RSTRING_PTR(string), RSTRING_LEN(string));
  return Qnil;
}


VALUE client_release(VALUE _, VALUE rb_client)
{
  ebb_client *client;
  Data_Get_Struct(rb_client, ebb_client, client);
  ebb_client_release(client);
  return Qnil;
}


void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  VALUE mFFI = rb_define_module_under(mEbb, "FFI");
  
  rb_define_const(mEbb, "VERSION", rb_str_new2(EBB_VERSION));
  
  /** Defines global strings in the init method. */
#define DEF_GLOBAL(N, val) global_##N = rb_obj_freeze(rb_str_new2(val)); rb_global_variable(&global_##N)
  DEF_GLOBAL(fragment, "FRAGMENT");
  DEF_GLOBAL(path_info, "PATH_INFO");
  DEF_GLOBAL(query_string, "QUERY_STRING");
  DEF_GLOBAL(request_body, "REQUEST_BODY");
  DEF_GLOBAL(request_method, "REQUEST_METHOD");  
  DEF_GLOBAL(request_path, "REQUEST_PATH");
  DEF_GLOBAL(request_uri, "REQUEST_URI");
  DEF_GLOBAL(server_port, "SERVER_PORT");
  DEF_GLOBAL(http_accept, "HTTP_ACCEPT");
  DEF_GLOBAL(http_connection, "HTTP_CONNECTION");
  DEF_GLOBAL(http_content_length, "HTTP_CONTENT_LENGTH");
  DEF_GLOBAL(http_content_type, "HTTP_CONTENT_TYPE");
  DEF_GLOBAL(http_prefix, "HTTP_");
  DEF_GLOBAL(http_version, "HTTP_VERSION");
  
  rb_define_singleton_method(mFFI, "server_process_connections", server_process_connections, 0);
  rb_define_singleton_method(mFFI, "server_listen_on_port", server_listen_on_port, 1);
  rb_define_singleton_method(mFFI, "server_unlisten", server_unlisten, 0);
  rb_define_singleton_method(mFFI, "server_open?", server_open, 0);
  
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  rb_define_singleton_method(mFFI, "client_read_input", client_read_input, 2);
  rb_define_singleton_method(mFFI, "client_write_status", client_write_status, 3);
  rb_define_singleton_method(mFFI, "client_write_header", client_write_header, 3);
  rb_define_singleton_method(mFFI, "client_write_body", client_write_body, 2);
  rb_define_singleton_method(mFFI, "client_env", client_env, 1);
  rb_define_singleton_method(mFFI, "client_release", client_release, 1);
  
  /* initialize ebb_server */
  loop = ev_default_loop (0);
  
  ev_idle_init (&idle_watcher, idle_cb);
  attach_idle_watcher();
  
  server = ebb_server_alloc();
  VALUE waiting_clients = rb_ary_new();
  rb_iv_set(mFFI, "@waiting_clients", waiting_clients);
  ebb_server_init(server, loop, request_cb, (void*)waiting_clients);
}
