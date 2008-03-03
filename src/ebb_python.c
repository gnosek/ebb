/* A python binding to the ebb web server
 * Copyright (c) 2008 Ry Dahl. This software is released under the MIT 
 * License. See README file for details.
 */
#include <Python.h>
#include <ebb.h>
#include <ev.h>

typedef struct {
    PyObject_HEAD
    ebb_server *server;
} Server;

void Server_alloc() 
{
  
}

static void
Server_dealloc(Server* self)
{
  ebb_server_free(self->server);
  self->ob_type->tp_free((PyObject*)self);
}


static PyMethodDef Server_methods[] = {
    {"name", (PyCFunction)Noddy_name, METH_NOARGS,
     "Return the name, combining the first and last name"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject ServerType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "ebb.Server",              /*tp_name*/
    sizeof(Server),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Server_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Ebb Server",           /* tp_doc */
    0,                   /* tp_traverse */
    0,                   /* tp_clear */
    0,                   /* tp_richcompare */
    0,                   /* tp_weaklistoffset */
    0,                   /* tp_iter */
    0,                   /* tp_iternext */
    Server_methods,             /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Server_init,      /* tp_init */
    Server_alloc,               /* tp_alloc */
    Server_new,                 /* tp_new */
};

initebb(void) 
{
  PyObject *m, *d, *tmp;
  ServerType.ob_type = &PyType_Type;

  m = Py_InitModule("Ebb", GeoIP_Class_methods);
  d = PyModule_GetDict(m);

  tmp = PyInt_FromLong(0);
  PyDict_SetItemString(d, "GEOIP_STANDARD", tmp);
  Py_DECREF(tmp);

  tmp = PyInt_FromLong(1);
  PyDict_SetItemString(d, "GEOIP_MEMORY_CACHE", tmp);
  Py_DECREF(tmp);
}