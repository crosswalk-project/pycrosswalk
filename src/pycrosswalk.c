// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Python.h>

#include "xwalk/XW_Extension.h"
#include "xwalk/XW_Extension_SyncMessage.h"

// XWalk hooks
const XW_MessagingInterface* g_xw_messaging = NULL;
const XW_Internal_SyncMessagingInterface* g_xw_sync_messaging = NULL;

// Python hooks
PyObject* g_py_messaging = NULL;
PyObject* g_py_sync_messaging = NULL;

char* g_extension_name = NULL;
char* g_javascript_api = NULL;

static PyObject* py_set_extension_name(PyObject* self, PyObject* args);
static PyObject* py_set_javascript_api(PyObject* self, PyObject* args);
static PyObject* py_post_message(PyObject* self, PyObject* args);
static PyObject* py_set_message_callback(PyObject* self, PyObject* args);
static PyObject* py_set_sync_message_callback(PyObject* self, PyObject* args);

static PyMethodDef PyXWalkMethods[] = {
  {"SetExtensionName", py_set_extension_name, METH_VARARGS, ""},
  {"SetJavaScriptAPI", py_set_javascript_api, METH_VARARGS, ""},
  {"PostMessage", py_post_message, METH_VARARGS, ""},
  {"SetMessageCallback", py_set_message_callback, METH_VARARGS, ""},
  {"SetSyncMessageCallback", py_set_sync_message_callback, METH_VARARGS, ""},
  {NULL, NULL, 0, NULL}
};

static struct PyModuleDef PyXWalkModule = {
  PyModuleDef_HEAD_INIT,
  "xwalk", "", -1, PyXWalkMethods,
  NULL, NULL, NULL, NULL
};

static PyObject* py_post_message(PyObject* self, PyObject* args) {
  int instance;
  char *result;

  if(!PyArg_ParseTuple(args, "is", &instance, &result)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }

  g_xw_messaging->PostMessage(instance, result);

  Py_RETURN_TRUE;
}

static PyObject* py_set_extension_name(PyObject* self, PyObject* args) {
  if (g_extension_name)
    Py_RETURN_FALSE;

  char* name = NULL;
  if(!PyArg_ParseTuple(args, "s", &name)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }

  g_extension_name = strdup(name);

  Py_RETURN_TRUE;
}

static PyObject* py_set_javascript_api(PyObject* self, PyObject* args) {
  if (g_javascript_api)
    Py_RETURN_FALSE;

  char* api = NULL;
  if(!PyArg_ParseTuple(args, "s", &api)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }

  g_javascript_api = strdup(api);

  Py_RETURN_TRUE;
}

static PyObject* py_set_message_callback(PyObject* self, PyObject* args) {
  if (g_py_messaging)
    Py_RETURN_FALSE;

  if(!PyArg_ParseTuple(args, "O", &g_py_messaging)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }
  Py_INCREF(g_py_messaging);

  Py_RETURN_TRUE;
}

static PyObject* py_set_sync_message_callback(PyObject* self, PyObject* args) {
  if (g_py_sync_messaging)
    Py_RETURN_FALSE;

  if(!PyArg_ParseTuple(args, "O", &g_py_sync_messaging)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }
  Py_INCREF(g_py_sync_messaging);

  Py_RETURN_TRUE;
}

static char* py_handle_message(XW_Instance instance,
                               PyObject* callback,
                               const char* message) {
  PyObject* instance_object = PyLong_FromLong((long) instance);
  PyObject* message_object = PyUnicode_FromString(message);
  PyObject* args = PyTuple_Pack(2, instance_object, message_object);
  Py_DECREF(message_object);
  Py_DECREF(instance_object);

  if (!args) {
    PyErr_Print();
    return NULL;
  }

  PyObject* result_object = PyObject_CallObject(callback, args);
  Py_DECREF(args);

  if (!result_object) {
    PyErr_Print();
    return NULL;
  }

  char* pass_string = NULL;

  char* result_string = PyUnicode_AsUTF8(result_object);
  if (result_string)
    pass_string = strdup(result_string);

  Py_DECREF(result_object);

  return pass_string;
}

static void xw_handle_message(XW_Instance instance, const char* message) {
  char* result = py_handle_message(instance, g_py_messaging, message);
  if (result)
    free(result);
}

static void xw_handle_sync_message(XW_Instance instance, const char* message) {
  char* result = py_handle_message(instance, g_py_sync_messaging, message);
  if (result) {
    g_xw_sync_messaging->SetSyncReply(instance, result);
    free(result);
  } else
    g_xw_sync_messaging->SetSyncReply(instance, "");
}

static void shutdown(XW_Extension extension) {
  Py_DECREF(g_py_sync_messaging);
  Py_DECREF(g_py_messaging);
  Py_Finalize();

  free(g_extension_name);
  free(g_javascript_api);
}

int32_t XW_Initialize(XW_Extension extension, XW_GetInterface get_interface) {
  // Hack to avoid missing symbols if the python script we are loading tries
  // to do something funny with cpython.
  void* handle = dlopen("libpython3.3m.so.1", RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    fprintf(stderr, "Could not load python shared library.\n");
    return XW_ERROR;
  }
  dlclose(handle);

  char* search_path_string = getenv("PYXWALK_PATH");
  if (!search_path_string) {
    fprintf(stderr, "PYXWALK_PATH not set.\n");
    return XW_ERROR;
  }

  Py_Initialize();
  PyObject* search_path_list = PySys_GetObject("path");
  PyObject* search_path_object = PyUnicode_FromString(search_path_string);

  PyList_Append(search_path_list, search_path_object);
  Py_DECREF(search_path_object);

  PyObject* xwalk_module = PyModule_Create(&PyXWalkModule);
  PyDict_SetItemString(PyImport_GetModuleDict(), PyXWalkModule.m_name, xwalk_module);

  PyObject* module = PyImport_ImportModule("extension");
  if (!module) {
    PyErr_Print();
    return XW_ERROR;
  }
  Py_DECREF(module);

  if (!g_extension_name || !g_javascript_api) {
    fprintf(stderr, "Extension name or JavaScript API not set.\n");
    return XW_ERROR;
  }

  const XW_CoreInterface* core = get_interface(XW_CORE_INTERFACE);
  core->SetExtensionName(extension, g_extension_name);
  core->SetJavaScriptAPI(extension, g_javascript_api);

  core->RegisterShutdownCallback(extension, shutdown);

  g_xw_messaging = get_interface(XW_MESSAGING_INTERFACE);
  g_xw_messaging->Register(extension, xw_handle_message);

  g_xw_sync_messaging = get_interface(XW_INTERNAL_SYNC_MESSAGING_INTERFACE);
  g_xw_sync_messaging->Register(extension, xw_handle_sync_message);

  return XW_OK;
}
