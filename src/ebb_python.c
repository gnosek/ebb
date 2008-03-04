/* A python binding to the ebb web server
 * Copyright (c) 2008 Ry Dahl. This software is released under the MIT 
 * License. See README file for details.
 */
#include <Python.h>
#include "ebb.h"
#include <ev.h>
#include <assert.h>

#define PyDict_SetStringString(dict, str1, str2) PyDict_SetItemString(dict, str1, PyString_FromString(str2))
#define ASCII_UPPER(ch) ('a' <= ch && ch <= 'z' ? ch - 'a' + 'A' : ch)

/* Why is there a global ebb_server variable instead of a wrapping it in a 
 * class? Because you would for no conceivable reason want to run more than
 * one ebb_server per python VM instance.
 */
static ebb_server server;
struct ev_loop *loop = NULL;
static PyObject *application = NULL;
static PyObject *base_env = NULL;
static PyObject *global_http_prefix;
static PyObject *global_request_method;
static PyObject *global_request_uri;
static PyObject *global_fragment;
static PyObject *global_request_path;
static PyObject *global_query_string;
static PyObject *global_http_version;
static PyObject *global_request_body;
static PyObject *global_server_name;
static PyObject *global_server_port;
static PyObject *global_path_info;
static PyObject *global_content_length;
static PyObject *global_http_host;

/* A callable type called Client. __call__(status, response_headers)
 * is the second argument to an appplcation
 * Client.environ is the first argument.
 */
typedef struct {
  PyObject_HEAD
  ebb_client *client;
} ebb_Client;


static PyObject* env_field(struct ebb_env_item *item)
{
  PyObject* f = NULL;
  int i;
  
  switch(item->type) {
    case EBB_FIELD_VALUE_PAIR:
      f = PyString_FromStringAndSize(NULL, PyString_GET_SIZE(global_http_prefix) + item->field_length);
      memcpy( PyString_AS_STRING(f)
            , PyString_AS_STRING(global_http_prefix)
            , PyString_GET_SIZE(global_http_prefix)
            );
      for(i = 0; i < item->field_length; i++) {
        char *ch = PyString_AS_STRING(f) + PyString_GET_SIZE(global_http_prefix) + i;
        *ch = item->field[i] == '-' ? '_' : ASCII_UPPER(item->field[i]);
      }
      break;
    case EBB_REQUEST_METHOD:  f = global_request_method; break;
    case EBB_REQUEST_URI:     f = global_request_uri; break;
    case EBB_FRAGMENT:        f = global_fragment; break;
    case EBB_REQUEST_PATH:    f = global_request_path; break;
    case EBB_QUERY_STRING:    f = global_query_string; break;
    case EBB_HTTP_VERSION:    f = global_http_version; break;
    case EBB_SERVER_NAME:     f = global_server_name; break;
    case EBB_SERVER_PORT:     f = global_server_port; break;
    case EBB_CONTENT_LENGTH:  f = global_content_length; break;
    default: assert(FALSE);
  }
  Py_INCREF(f);
  return f;
}


static PyObject* env_value(struct ebb_env_item *item)
{
  if(item->value_length > 0)
    return PyString_FromStringAndSize(item->value, item->value_length);
  else
    return Py_None; // XXX need to increase ref count? :/
}


static PyObject* client_env(ebb_client *client)
{
  PyObject *env = PyDict_Copy(base_env);
  int i;
  
  for(i=0; i < client->env_size; i++) {
    PyDict_SetItem(env, env_field(&client->env[i])
                      , env_value(&client->env[i])
                      );
  }
  // PyDict_SetStringString(hash, global_path_info, rb_hash_aref(hash, global_request_path));
  
  return env;
}

const char *test_response = "Hello World!\r\n";

void request_cb(ebb_client *client, void *ignore)
{
  PyObject *env = client_env(client);
  PyObject *start_response = Py_None;
  PyObject *arglist; 
  PyObject *result;
  
  printf("hello world\n");
  
  arglist = Py_BuildValue("OO", env, start_response);
  result = PyEval_CallObject(application, arglist);
  
  Py_DECREF(arglist);
  Py_DECREF(env);
  
  ebb_client_write(client, test_response, strlen(test_response));
  ebb_client_finished(client);
  
  printf("finished calling\n");
}


static PyObject *start_server(PyObject *self, PyObject *args)
{
  PyObject *application_temp;
  int port;
  
  if(!PyArg_ParseTuple(args, "Oi", &application_temp, &port))
    return NULL;
  if(!PyCallable_Check(application_temp)) {
    PyErr_SetString(PyExc_TypeError, "parameter must be callable");
    return NULL;
  }
  Py_XINCREF(application_temp);         /* Add a reference to new callback */
  Py_XDECREF(application);              /* Dispose of previous callback */
  application = application_temp;       /* Remember new callback */
  
  loop = ev_default_loop(0);
  ebb_server_init(&server, loop, request_cb, NULL);
  ebb_server_listen_on_port(&server, port);
  
  //ev_loop(loop, EVLOOP_ONESHOT);
  ev_loop(loop, 0);
  
  Py_XDECREF(application);
  
  Py_RETURN_NONE;
}

static PyObject *stop_server(PyObject *self)
{
  Py_RETURN_NONE;
}

static PyMethodDef Ebb_methods[] = 
  { {"start_server" , (PyCFunction)start_server, METH_VARARGS, 
     "Listen on port supplied. Start the server event loop." }
  , {"stop_server" , (PyCFunction)stop_server, METH_NOARGS, 
     "Unlisten." }
  , {NULL, NULL, 0, NULL}
  };

PyMODINIT_FUNC initebb(void) 
{
  ebb_server_init(&server, loop, request_cb, NULL);
  
  
  PyObject *m;
  
  m = Py_InitModule("ebb", Ebb_methods);

  base_env = PyDict_New();
  PyDict_SetStringString(base_env, "SCRIPT_NAME", "");
  PyDict_SetStringString(base_env, "SERVER_SOFTWARE", "Ebb 0.0.4");
  PyDict_SetStringString(base_env, "SERVER_PROTOCOL", "HTTP/1.1");
  PyDict_SetStringString(base_env, "wsgi.url_scheme", "http");
  PyDict_SetItemString(base_env, "wsgi.multithread", Py_False);
  PyDict_SetItemString(base_env, "wsgi.multiprocess", Py_False);
  PyDict_SetItemString(base_env, "wsgi.run_once", Py_False);
  //PyDict_SetItemString(base_env, "wsgi.version", (0,1));
  //PyDict_SetItemString(base_env, "wsgi.errors", STDERR);
  
#define DEF_GLOBAL(N, val) global_##N = PyString_FromString(val)
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
  
}