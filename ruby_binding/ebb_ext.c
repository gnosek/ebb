/* A ruby binding to the ebb web server
 * Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
 * This software is released under the "MIT License". See README file for details.
 */

#include <ruby.h>
#include <ebb.h>
#include <tcp.h>
#include <ev.h>

static VALUE cServer;
static VALUE cClient;

/* Variables with an underscore are C-level variables */

VALUE client_new(ebb_client*);

VALUE server_alloc(VALUE self)
{
  struct ev_loop *loop = ev_default_loop (0);
  ebb_server *_server = ebb_server_new(loop);
  VALUE server = Qnil;
  server = Data_Wrap_Struct(cServer, 0, ebb_server_free, _server);
  return server; 
}

void request_cb(ebb_client *_client, void *data)
{
  VALUE server = (VALUE)data;
  VALUE waiting_clients;
  VALUE client = client_new(_client);
  
  waiting_clients = rb_iv_get(server, "@waiting_clients");
  rb_ary_push(waiting_clients, client);
}

VALUE server_start_listening(VALUE server)
{
  ebb_server *_server;
  VALUE host, port;
  
  Data_Get_Struct(server, ebb_server, _server);
  
  host = rb_iv_get(server, "@host");
  Check_Type(host, T_STRING);
  port = rb_iv_get(server, "@port");
  Check_Type(port, T_FIXNUM);
  rb_iv_set(server, "@waiting_clients", rb_ary_new());
  
  ebb_server_start(_server, StringValuePtr(host), FIX2INT(port), request_cb, (void*)server);
  
  return Qnil;
}

VALUE server_process_connections(VALUE server)
{
  ebb_server *_server;
  VALUE host, port;
  
  Data_Get_Struct(server, ebb_server, _server);
  /* FIXME: don't go inside internal datastructures */
  ev_loop(_server->socket->loop, EVLOOP_NONBLOCK);
  
  
  /* FIXME: Need way to know when the loop is finished...
   * should return true or false */
  
  if(_server->socket->open)
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

VALUE client_new(ebb_client *_client)
{
  return Data_Wrap_Struct(cClient, 0, ebb_client_free, _client);
}

VALUE client_write(VALUE client, VALUE string)
{
  ebb_client *_client;
  int written;
  
  Data_Get_Struct(client, ebb_client, _client);
  written = ebb_client_write(_client, RSTRING_PTR(string), RSTRING_LEN(string));
  return INT2FIX(written);
}

VALUE client_env(VALUE client)
{
  ebb_client *_client;
  VALUE hash = rb_hash_new();
  int i;
  
  Data_Get_Struct(client, ebb_client, _client);
  for(i=0; i < _client->env_size; i++) {
    rb_hash_aset(hash
                , rb_str_new(_client->env_fields[i], _client->env_field_lengths[i])
                , rb_str_new(_client->env_values[i], _client->env_value_lengths[i])
                );
  }
  return hash;
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
  
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "start_listening", server_start_listening, 0);
  rb_define_method(cServer, "process_connections", server_process_connections, 0);
  rb_define_method(cServer, "stop", server_stop, 0);
  
  rb_define_method(cClient, "write", client_write, 1);
  rb_define_method(cClient, "env", client_env, 0);
  rb_define_method(cClient, "close", client_close, 0);
}