/* A ruby binding to the ebb web server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */

#include <ruby.h>
#include <assert.h>
#include <ebb.h>
#include <ev.h>

static VALUE cServer;
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


/* Variables with an underscore are C-level variables */

VALUE env_field(const char *field, int length)
{
  VALUE f;
  char *ch, *end;
  
  if(field == NULL) {
    switch(length) {
      case EBB_REQUEST_METHOD:  return global_request_method;
      case EBB_REQUEST_URI:     return global_request_uri;
      case EBB_FRAGMENT:        return global_fragment;
      case EBB_REQUEST_PATH:    return global_request_path;
      case EBB_QUERY_STRING:    return global_query_string;
      case EBB_HTTP_VERSION:    return global_http_version;
      case EBB_SERVER_NAME:     return global_server_name;
      case EBB_SERVER_PORT:     return global_server_port;
      case EBB_CONTENT_LENGTH:  return global_content_length;
      default: assert(FALSE); /* unknown const */
    }
  } else {
    f = rb_str_dup(global_http_prefix);
    f = rb_str_buf_cat(f, field, length); 
    
    for(ch = RSTRING_PTR(f), end = ch + RSTRING_LEN(f); ch < end; ch++) {
      if(*ch == '-') {
        *ch = '_';
      } else {
        *ch = toupper(*ch);
      }
    }
    return f;
  }
  assert(FALSE);
  return Qnil;
}

VALUE client_env(ebb_client *_client)
{
  VALUE hash = rb_hash_new();
  int i;
  /* This client->env_fields, client->env_value structure is pretty hacky
   * and a bit hard to follow. Look at the #defines at the top of ebb.c to
   * see what they are doing. Basically it's a list of (ptr,length) pairs
   * for both a field and value
   */
  for(i=0; i < _client->env_size; i++) {
    rb_hash_aset(hash, env_field(_client->env_fields[i], _client->env_field_lengths[i])
                     , rb_str_new(_client->env_values[i], _client->env_value_lengths[i])
                     );
  }
  rb_hash_aset(hash, global_path_info, rb_hash_aref(hash, global_request_path));
  return hash;
}

VALUE client_new(ebb_client *_client)
{
  VALUE client = Data_Wrap_Struct(cClient, 0, 0, _client);
  _client->data = (void*)client;
  // if(_client->upload_file_filename)
  //   rb_iv_set(client, "@upload_filename", rb_str_new2(_client->upload_file_filename));
  rb_iv_set(client, "@ebb_env", client_env(_client));
  return client;
}

void request_cb(ebb_client *_client, void *data)
{
  VALUE server = (VALUE)data;
  VALUE waiting_clients;
  VALUE client = client_new(_client);
  
  waiting_clients = rb_iv_get(server, "@waiting_clients");
  rb_ary_push(waiting_clients, client);
}

VALUE server_alloc(VALUE self)
{
  ebb_server *_server = ebb_server_alloc();
  VALUE server = Qnil;
  server = Data_Wrap_Struct(cServer, 0, ebb_server_free, _server);
  return server; 
}


VALUE server_init(VALUE server, VALUE host, VALUE port)
{
  struct ev_loop *loop = ev_default_loop (0);
  ebb_server *_server;
  
  Data_Get_Struct(server, ebb_server, _server);
  ebb_server_init(_server, loop, StringValuePtr(host), FIX2INT(port), request_cb, (void*)server);
  
  

  return Qnil;
}

VALUE server_listen(VALUE server)
{
  ebb_server *_server;
  
  Data_Get_Struct(server, ebb_server, _server);
  rb_iv_set(server, "@waiting_clients", rb_ary_new());
  ebb_server_listen(_server);
  return Qnil;
}

static void
oneshot_timeout (struct ev_loop *loop, struct ev_timer *w, int revents) {;}

VALUE server_process_connections(VALUE server)
{
  ebb_server *_server;
  VALUE host, port;
  
  Data_Get_Struct(server, ebb_server, _server);
  
  
  ev_timer timeout;
  ev_timer_init (&timeout, oneshot_timeout, 0.5, 0.);
  ev_timer_start (_server->loop, &timeout);
   
  ev_loop(_server->loop, EVLOOP_ONESHOT);
  /* XXX: Need way to know when the loop is finished...
   * should return true or false */
   
  ev_timer_stop(_server->loop, &timeout);
  
  if(_server->open)
    return Qtrue;
  else
    return Qfalse;
}

VALUE server_unlisten(VALUE server)
{
  ebb_server *_server;
  Data_Get_Struct(server, ebb_server, _server);
  ebb_server_unlisten(_server);
  return Qnil;
}

VALUE client_write(VALUE client, VALUE string)
{
  ebb_client *_client;
  int written;
  
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_write(_client, RSTRING_PTR(string), RSTRING_LEN(string));
  return Qnil;
}

VALUE client_finished(VALUE client)
{
  ebb_client *_client;
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_finished(_client);
  return Qnil;
}

VALUE client_close(VALUE client)
{
  ebb_client *_client;

  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_close(_client);
  return Qnil;
}

VALUE client_read_input(VALUE client, VALUE size)
{
  ebb_client *_client;
  GString *_string;
  VALUE string;
  int _size = FIX2INT(size);
  int nread;
  
  Data_Get_Struct(client, ebb_client, _client);
  
  string = rb_str_buf_new( _size );
  nread = ebb_client_read(_client, RSTRING_PTR(string), _size);
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

VALUE client_init(VALUE self, VALUE something) {return self;}

void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  cServer = rb_define_class_under(mEbb, "Server", rb_cObject);
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  
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
  DEF_GLOBAL(content_length, "HTTP_CONTENT_LENGTH");
  DEF_GLOBAL(http_host, "HTTP_HOST");
  
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "init", server_init, 2);
  rb_define_method(cServer, "process_connections", server_process_connections, 0);
  rb_define_method(cServer, "listen", server_listen, 0);
  rb_define_method(cServer, "unlisten", server_unlisten, 0);
  
  rb_define_method(cClient, "initialize", client_init, 1);
  rb_define_method(cClient, "write", client_write, 1);
  rb_define_method(cClient, "read_input", client_read_input, 1);
  rb_define_method(cClient, "finished", client_finished, 0);
  
  rb_define_method(cClient, "read_input", client_read_input, 1);
  
}
