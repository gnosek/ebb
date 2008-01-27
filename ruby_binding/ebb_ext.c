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

static VALUE global_request_method;
static VALUE global_request_uri;
static VALUE global_fragment;
static VALUE global_request_path;
static VALUE global_query_string;
static VALUE global_http_version;
static VALUE global_request_body;
static VALUE global_server_name;
static VALUE global_server_port;

/* Variables with an underscore are C-level variables */

VALUE env_field(const char *field, int length)
{
  if(field == NULL)
    switch(length) {
      case EBB_REQUEST_METHOD:  return global_request_method;
      case EBB_REQUEST_URI:     return global_request_uri;
      case EBB_FRAGMENT:        return global_fragment;
      case EBB_REQUEST_PATH:    return global_request_path;
      case EBB_QUERY_STRING:    return global_query_string;
      case EBB_HTTP_VERSION:    return global_http_version;
      case EBB_REQUEST_BODY:    return global_request_body;
      case EBB_SERVER_NAME:     return global_server_name;
      case EBB_SERVER_PORT:     return global_server_port;
      default: assert(FALSE); /* unknown const */
    }
  else
    return rb_str_new(field,length); /* TODO add prefix */
}

VALUE env_value(const char *field, int length)
{
  /* for now just this, but later more */
  return rb_str_new(field,length);
}

VALUE client_env(VALUE client)
{
  ebb_client *_client;
  VALUE hash = rb_hash_new();
  int i;
  
  Data_Get_Struct(client, ebb_client, _client);
  for(i=0; i < _client->env_size; i++) {
    rb_hash_aset(hash, env_field(_client->env_fields[i], _client->env_field_lengths[i])
                     , env_value(_client->env_values[i], _client->env_value_lengths[i])
                     );
  }
  return hash;
}

VALUE client_new(ebb_client *_client)
{
  VALUE client = Data_Wrap_Struct(cClient, 0, ebb_client_close, _client);
  
  rb_iv_set(client, "@env", client_env(client));
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

VALUE server_start(VALUE server)
{
  ebb_server *_server;
  
  Data_Get_Struct(server, ebb_server, _server);
  rb_iv_set(server, "@waiting_clients", rb_ary_new());
  ebb_server_start(_server);
  return Qnil;
}

VALUE server_process_connections(VALUE server)
{
  ebb_server *_server;
  VALUE host, port;
  
  Data_Get_Struct(server, ebb_server, _server);
  //ev_loop(_server->loop, EVLOOP_NONBLOCK);
  ev_loop(_server->loop, EVLOOP_ONESHOT);
  /* XXX: Need way to know when the loop is finished...
   * should return true or false */
  if(_server->open)
    return Qtrue;
  else
    return Qfalse;
}

VALUE server_stop(VALUE server)
{
  ebb_server *_server;
  Data_Get_Struct(server, ebb_server, _server);
  ebb_server_stop(_server);
  return Qnil;
}

VALUE client_write(VALUE client, VALUE string)
{
  ebb_client *_client;
  int written;
  
  Data_Get_Struct(client, ebb_client, _client);
  written = ebb_client_write(_client, RSTRING_PTR(string), RSTRING_LEN(string));
  return INT2FIX(written);
}

VALUE client_close(VALUE client)
{
  ebb_client *_client;

  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_close(_client);
  return Qnil;
}


void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  cServer = rb_define_class_under(mEbb, "Server", rb_cObject);
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  
#define DEF_GLOBAL(N, val)   global_##N = rb_obj_freeze(rb_str_new2(val)); rb_global_variable(&global_##N)
  DEF_GLOBAL(request_method, "REQUEST_METHOD");  
  DEF_GLOBAL(request_uri, "REQUEST_URI");
  DEF_GLOBAL(fragment, "FRAGMENT");
  DEF_GLOBAL(request_path, "REQUEST_PATH");
  DEF_GLOBAL(query_string, "QUERY_STRING");
  DEF_GLOBAL(http_version, "HTTP_VERSION");
  DEF_GLOBAL(request_body, "REQUEST_BODY");
  DEF_GLOBAL(server_name, "SERVER_NAME");
  DEF_GLOBAL(server_port, "SERVER_PORT");
  
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "init", server_init, 2);
  rb_define_method(cServer, "process_connections", server_process_connections, 0);
  rb_define_method(cServer, "_start", server_start, 0);
  rb_define_method(cServer, "stop", server_stop, 0);
  
  rb_define_method(cClient, "write", client_write, 1);
  rb_define_method(cClient, "close", client_close, 0);
}
