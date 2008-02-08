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
static VALUE global_path_info;
static VALUE global_content_length;


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
      case EBB_SERVER_NAME:     return global_server_name;
      case EBB_SERVER_PORT:     return global_server_port;
      case EBB_CONTENT_LENGTH:  return global_content_length;
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
  
  rb_hash_aset(hash, global_path_info, rb_hash_aref(hash, global_request_path));
  
  return hash;
}

VALUE client_new(ebb_client *_client)
{
  VALUE client = Data_Wrap_Struct(cClient, 0, 0, _client);
  
  _client->data = (void*)client;
  
  rb_iv_set(client, "@env", client_env(client));
  rb_iv_set(client, "@write_buffer", rb_ary_new());
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
  ebb_client_write(_client, RSTRING_PTR(string), RSTRING_LEN(string));
  return Qnil;
}

VALUE client_start_writing(VALUE client)
{
  ebb_client *_client;
  
  Data_Get_Struct(client, ebb_client, _client);
  ebb_client_start_writing(_client, NULL);
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
  RSTRING_LEN(string) = nread;
  
  if(nread < 0)
    rb_raise(rb_eRuntimeError,"There was a problem reading from input (bad tmp file?)");
  
  if(nread == 0)
    return Qnil;
  
  return string;
}


void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  cServer = rb_define_class_under(mEbb, "Server", rb_cObject);
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  
/** Defines global strings in the init method. */
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
  DEF_GLOBAL(path_info, "PATH_INFO");
  DEF_GLOBAL(content_length, "HTTP_CONTENT_LENGTH");
  
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "init", server_init, 2);
  rb_define_method(cServer, "process_connections", server_process_connections, 0);
  rb_define_method(cServer, "_start", server_start, 0);
  rb_define_method(cServer, "stop", server_stop, 0);
  
  rb_define_method(cClient, "write", client_write, 1);
  rb_define_method(cClient, "start_writing", client_start_writing, 0);
  rb_define_method(cClient, "read_input", client_read_input, 1);
}
