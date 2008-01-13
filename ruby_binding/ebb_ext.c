#include <ruby.h>
#include "../ebb.h"

static VALUE cServer;
static VALUE cClient;

/* Variables with an underscore are C-level variables */

VALUE server_alloc(VALUE self) {
  ebb_server *_server = ebb_server_new();
  VALUE server = Qnil;
  server = Data_Wrap_Struct(cServer, 0, ebb_server_free, _server);
  return server; 
}

void request_cb(ebb_client *request, void *data)
{
  VALUE server = (VALUE)data;
  VALUE client = client_new(request);
  
  rb_funcall(server, rb_intern("process"), 1, client);
}

VALUE server_start(VALUE server) {
  ebb_server *_server;
  VALUE host, port;
  
  Data_Get_Struct(server, ebb_server, _server);
  
  host = rb_iv_get(server, "host");
  Check_Type(host, T_STRING);
  port = rb_iv_get(server, "port");
  Check_Type(port, T_FIXNUM);
  
  ebb_server_start(_server, StringValuePtr(host), FIX2INT(port), request_cb, (void*)server);
  
  return Qnil;
}

VALUE server_stop(VALUE server) {
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
  
  Data_Get_Struct(client, ebb_client, _client);
  tcp_client_write(_request->socket, );
}

void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  cServer = rb_define_class_under(mEbb, "Server", rb_cObject);
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "start", server_start, 0);
  rb_define_method(cServer, "stop", server_stop, 0);
  
  rb_define_method(cClient, "write", rb_ebb_client_write, 1);
  rb_define_method(cClient, "env", rb_ebb_client_env, 0);
  rb_define_method(cClient, "close", rb_ebb_client_close, 0);
}