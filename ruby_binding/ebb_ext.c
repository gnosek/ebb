#include <ruby.h>
#include <ebb.h>
#include <tcp.h>

static VALUE cServer;
static VALUE cClient;

/* Variables with an underscore are C-level variables */

VALUE client_new(ebb_client*);

VALUE server_alloc(VALUE self)
{
  ebb_server *_server = ebb_server_new();
  VALUE server = Qnil;
  server = Data_Wrap_Struct(cServer, 0, ebb_server_free, _server);
  return server; 
}

void request_cb(ebb_client *_client, void *data)
{
  VALUE server = (VALUE)data;
  VALUE client = client_new(_client);
  rb_funcall(server, rb_intern("process"), 1, client);
}

VALUE server_start(VALUE server)
{
  ebb_server *_server;
  VALUE host, port;
  
  Data_Get_Struct(server, ebb_server, _server);
  
  host = rb_iv_get(server, "@host");
  Check_Type(host, T_STRING);
  port = rb_iv_get(server, "@port");
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
  int written;
  
  Data_Get_Struct(client, ebb_client, _client);
  written = tcp_client_write(_client->socket, RSTRING_PTR(string), RSTRING_LEN(string));
  return INT2FIX(written);
}

VALUE client_env(VALUE client)
{
  ebb_client *_client;
  VALUE hash = rb_hash_new();
  ebb_env_pair *pair;
  
  Data_Get_Struct(client, ebb_client, _client);
  while((pair = g_queue_pop_head(_client->env))) {
    rb_hash_aset(hash
                , rb_str_new(pair->field, pair->flen) // use symbol?
                , rb_str_new(pair->value, pair->vlen)
                );
  }
  return hash;
}

VALUE client_close(VALUE client)
{
  ebb_client *_client;
  
  Data_Get_Struct(client, ebb_client, _client);
  tcp_client_close(_client->socket); // TODO: ebb_client_close?
  return Qnil;
}

void Init_ebb_ext()
{
  VALUE mEbb = rb_define_module("Ebb");
  cServer = rb_define_class_under(mEbb, "Server", rb_cObject);
  cClient = rb_define_class_under(mEbb, "Client", rb_cObject);
  
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "_start", server_start, 0);
  rb_define_method(cServer, "stop", server_stop, 0);
  
  rb_define_method(cClient, "write", client_write, 1);
  rb_define_method(cClient, "env", client_env, 0);
  rb_define_method(cClient, "close", client_close, 0);
}