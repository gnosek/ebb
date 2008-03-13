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
struct ev_loop *loop;
static PyObject *application;
static PyObject *base_env;
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
} py_client;

static PyObject *
py_start_response(py_client *self, PyObject *args, PyObject *kw);

static void py_client_dealloc(PyObject *obj)
{
  py_client *self = (py_client*) obj;
  ebb_client_release(self->client);
  obj->ob_type->tp_free(obj);
  printf("dealloc called!\n");
}

static PyMethodDef client_methods[] = 
  { {"start_response" , (PyCFunction)py_start_response, METH_VARARGS, NULL }
  // , {"write" , (PyCFunction)write, METH_VARARGS, NULL }
  , {NULL, NULL, 0, NULL}
  };

static PyTypeObject py_client_t = 
  { ob_refcnt: 1
  , tp_name: "ebb.Client"
  , tp_doc: "a wrapper around ebb_client"
  , tp_basicsize: sizeof(py_client)
  , tp_flags: Py_TPFLAGS_DEFAULT
  , tp_methods: client_methods
  , tp_dealloc: py_client_dealloc
  };

static PyObject *
py_start_response(py_client *self, PyObject *args, PyObject *kw)
{
  PyObject *response_headers;
  char *status_string;
  int status;
  
  if(!PyArg_ParseTuple(args, "sO", &status_string, &response_headers))
    return NULL;
  
  /* do this goofy split(' ') operation. wsgi is such a terrible api. */
  status = 100 * (status_string[0] - '0') 
          + 10 * (status_string[1] - '0')
           + 1 * (status_string[2] - '0');
  assert(0 <= status && status < 1000);
  
  ebb_client_write_status(self->client, status, status_string+4);
  
  PyObject *iterator = PyObject_GetIter(response_headers);
  PyObject *header_pair;
  char *field, *value;
  while(header_pair = PyIter_Next(iterator)) {
    if(!PyArg_ParseTuple(header_pair, "ss", &field, &value))
      return NULL;
    ebb_client_write_header(self->client, field, value);
    Py_DECREF(header_pair);
  }
  ebb_client_write(self->client, "\r\n", 2);
  
  Py_RETURN_NONE;
}

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


static PyObject* py_client_env(ebb_client *client)
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

static py_client* py_client_new(ebb_client *client)
{
  py_client *self = PyObject_New(py_client, &py_client_t);
  if(self == NULL) return NULL;
  self->client = client;
  
  //if(0 < PyObject_SetAttrString((PyObject*)self, "environ", py_client_env(client)))
  //  return NULL;
  
  return self;
}

const char *test_response = "test_response defined in ebb_python.c\r\n";

void request_cb(ebb_client *client, void *ignore)
{
  
  PyObject *environ, *start_response;
  
  py_client *pclient = py_client_new(client);
  assert(pclient != NULL);
  //environ = PyObject_GetAttrString((PyObject*)pclient, "environ");  
  environ = py_client_env(client);
  assert(environ != NULL);
  
  start_response = PyObject_GetAttrString((PyObject*)pclient, "start_response");
  assert(start_response != NULL);
  
  PyObject *arglist = Py_BuildValue("OO", environ, start_response);
  assert(arglist != NULL);
  assert(application != NULL);
  PyObject *body = PyEval_CallObject(application, arglist);
  assert(body != NULL);
  
  Py_DECREF(arglist);
  Py_DECREF(environ);
  
  PyObject *iterator = PyObject_GetIter(body);
  PyObject *body_item;
  while (body_item = PyIter_Next(iterator)) {
    char *body_string = PyString_AsString(body_item);
    int body_length = PyString_Size(body_item);
    /* Todo support streaming! */
    ebb_client_write(pclient->client, body_string, body_length);
    Py_DECREF(body_item);
  }
  
  ebb_client_begin_transmission(client);
  
  Py_DECREF(pclient);
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


static PyMethodDef ebb_module_methods[] = 
  { {"start_server" , (PyCFunction)start_server, METH_VARARGS, NULL }
  , {"stop_server" , (PyCFunction)stop_server, METH_NOARGS, NULL }
  , {NULL, NULL, 0, NULL}
  };

PyMODINIT_FUNC initebb(void) 
{
  PyObject *m = Py_InitModule("ebb", ebb_module_methods);
  
  base_env = PyDict_New();
  PyDict_SetStringString(base_env, "SCRIPT_NAME", "");
  PyDict_SetStringString(base_env, "SERVER_SOFTWARE", EBB_VERSION);
  PyDict_SetStringString(base_env, "SERVER_NAME", "0.0.0.0");
  PyDict_SetStringString(base_env, "SERVER_PROTOCOL", "HTTP/1.1");
  PyDict_SetStringString(base_env, "wsgi.url_scheme", "http");
  PyDict_SetItemString(base_env, "wsgi.multithread", Py_False);
  PyDict_SetItemString(base_env, "wsgi.multiprocess", Py_False);
  PyDict_SetItemString(base_env, "wsgi.run_once", Py_False);
  //PyDict_SetItemString(base_env, "wsgi.version", (0,1));
  //PyDict_SetItemString(base_env, "wsgi.errors", STDERR);
  
  
  /* StartResponse */
  py_client_t.tp_new = PyType_GenericNew;
  if (PyType_Ready(&py_client_t) < 0) return;
  Py_INCREF(&py_client_t);
  PyModule_AddObject(m, "Client", (PyObject *)&py_client_t);
  
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
  
  ebb_server_init(&server, loop, request_cb, NULL);
}