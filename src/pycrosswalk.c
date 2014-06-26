// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Python.h>
#include <callback.h>

#include "xwalk/XW_Extension.h"
#include "xwalk/XW_Extension_Runtime.h"
#include "xwalk/XW_Extension_SyncMessage.h"

#define EXTENSION_MAX 64

// XWalk hooks
static const XW_MessagingInterface* g_xw_messaging = NULL;
static const XW_Internal_SyncMessagingInterface* g_xw_sync_messaging = NULL;

// Python hooks
static PyObject* g_py_messaging[EXTENSION_MAX];
static PyObject* g_py_sync_messaging[EXTENSION_MAX];
static PyObject* g_py_instance_created = NULL;
static PyObject* g_py_instance_destroyed = NULL;

static char* g_extension_name = NULL;
static char* g_javascript_api = NULL;

static PyObject* py_set_extension_name(PyObject* self, PyObject* args);
static PyObject* py_set_javascript_api(PyObject* self, PyObject* args);
static PyObject* py_post_message(PyObject* self, PyObject* args);
static PyObject* py_set_message_callback(PyObject* self, PyObject* args);
static PyObject* py_set_sync_message_callback(PyObject* self, PyObject* args);
static PyObject* py_set_instance_created_callback(PyObject* self, PyObject* args);
static PyObject* py_set_instance_destroyed_callback(PyObject* self, PyObject* args);

static PyMethodDef PyXWalkMethods[] = {
  {"SetExtensionName", py_set_extension_name, METH_VARARGS, ""},
  {"SetJavaScriptAPI", py_set_javascript_api, METH_VARARGS, ""},
  {"PostMessage", py_post_message, METH_VARARGS, ""},
  {"SetMessageCallback", py_set_message_callback, METH_VARARGS, ""},
  {"SetSyncMessageCallback", py_set_sync_message_callback, METH_VARARGS, ""},
  {"SetInstanceCreatedCallback", py_set_instance_created_callback, METH_VARARGS, ""},
  {"SetInstanceDestroyedCallback", py_set_instance_destroyed_callback, METH_VARARGS, ""},
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
  PyObject* callback = NULL;
  int instance = 0;

  if(!PyArg_ParseTuple(args, "lO", &instance, &callback)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }

  if (instance > EXTENSION_MAX || g_py_messaging[instance])
    Py_RETURN_FALSE;

  Py_INCREF(callback);
  g_py_messaging[instance] = callback;

  Py_RETURN_TRUE;
}

static PyObject* py_set_sync_message_callback(PyObject* self, PyObject* args) {
  PyObject* callback = NULL;
  int instance = 0;

  if(!PyArg_ParseTuple(args, "lO", &instance, &callback)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }

  if (instance > EXTENSION_MAX || g_py_sync_messaging[instance])
    Py_RETURN_FALSE;

  Py_INCREF(callback);
  g_py_sync_messaging[instance] = callback;

  Py_RETURN_TRUE;
}

static PyObject* py_set_instance_created_callback(PyObject* self, PyObject* args) {
  if (g_py_instance_created)
    Py_RETURN_FALSE;

  if(!PyArg_ParseTuple(args, "O", &g_py_instance_created)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }
  Py_INCREF(g_py_instance_created);

  Py_RETURN_TRUE;
}

static PyObject* py_set_instance_destroyed_callback(PyObject* self, PyObject* args) {
  if (g_py_instance_destroyed)
    Py_RETURN_FALSE;

  if(!PyArg_ParseTuple(args, "O", &g_py_instance_destroyed)) {
    PyErr_Print();
    Py_RETURN_FALSE;
  }
  Py_INCREF(g_py_instance_destroyed);

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
  if (!g_py_messaging[instance])
    return;

  char* result = py_handle_message(instance, g_py_messaging[instance], message);
  if (result)
    free(result);
}

static void xw_handle_sync_message(XW_Instance instance, const char* message) {
  if (!g_py_sync_messaging[instance]) {
    g_xw_sync_messaging->SetSyncReply(instance, "");
    return;
  }

  char* result = py_handle_message(instance, g_py_sync_messaging[instance], message);
  if (result) {
    g_xw_sync_messaging->SetSyncReply(instance, result);
    free(result);
  } else
    g_xw_sync_messaging->SetSyncReply(instance, "");
}

static char* py_handle_instance(XW_Instance instance, PyObject* callback) {
  if (!callback) {
    fprintf(stderr, "Handle instance (created/destroyed) not set!\n");
    return;
  }

  PyObject* instance_object = PyLong_FromLong((long) instance);
  PyObject* args = PyTuple_Pack(1, instance_object);
  Py_DECREF(instance_object);

  if (!args) {
    PyErr_Print();
    return;
  }

  PyObject* result_object = PyObject_CallObject(callback, args);
  Py_DECREF(args);

  if (!result_object)
    PyErr_Print();

  return;
}

static void xw_handle_shutdown(XW_Extension extension) {
  Py_Finalize();
}

static int load_python_extension(XW_Extension extension,
                                  XW_GetInterface get_interface) {
  const XW_Internal_RuntimeInterface* runtime =
      get_interface(XW_INTERNAL_RUNTIME_INTERFACE);

  char extension_path[4096];
  runtime->GetRuntimeVariableString(
      extension, "extension_path", extension_path, sizeof(extension_path));

  if (strlen(extension_path) == 0) {
    fprintf(stderr, "Runtime variable 'extension_path' not set.\n");
    return 0;
  }

  // Removed the "quotes" added by the extension framework (JSON wrapper).
  size_t extension_path_size = strlen(extension_path);
  strncpy(extension_path, &extension_path[1], extension_path_size);
  extension_path[extension_path_size - 2] = '\0';

  char* extension_plugin_name = basename(extension_path);
  size_t extension_plugin_name_size = strlen(extension_plugin_name);

  // Remove the lib prefix and the .so extension from the file name.
  // This can probably be done in a more portable way calling the python
  // file path manipulation libraries directly. Currently will only
  // work on Linux.
  char module_name[1024];
  strncpy(module_name, &extension_plugin_name[3],
      extension_plugin_name_size - 6);
  module_name[extension_plugin_name_size - 6] = '\0';

  PyObject* search_path_list = PySys_GetObject("path");
  PyObject* search_path_object = PyUnicode_FromString(dirname(extension_path));

  PyList_Append(search_path_list, search_path_object);
  Py_DECREF(search_path_object);

  PyObject* module = PyImport_ImportModule(module_name);
  if (!module) {
    PyErr_Print();
    return 0;
  }
  Py_DECREF(module);

  return 1;
}

static void instance_closure(PyObject* callback, va_alist args) {
  va_start_void(args);

  int instance = va_arg_int(args);
  py_handle_instance(instance, callback);

  va_return_void(args);
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

  Py_Initialize();

  PyObject* xwalk_module = PyModule_Create(&PyXWalkModule);
  PyDict_SetItemString(PyImport_GetModuleDict(), PyXWalkModule.m_name, xwalk_module);

  if (!load_python_extension(extension, get_interface))
      return XW_ERROR;

  if (!g_extension_name || !g_javascript_api) {
    fprintf(stderr, "Extension name or JavaScript API not set.\n");
    return XW_ERROR;
  }

  const XW_CoreInterface* core = get_interface(XW_CORE_INTERFACE);

  core->SetExtensionName(extension, g_extension_name);
  free(g_extension_name);
  g_extension_name = NULL;

  core->SetJavaScriptAPI(extension, g_javascript_api);
  free(g_javascript_api);
  g_javascript_api = NULL;

  // We need to create a closure here otherwise the callback is going to be
  // replaced by the next python extension, which makes it impossible to
  // support multiple extensions otherwise. I'm leaking the closure structure
  // and the callback object, but their lifecycle is the same as the extension
  // process (so effectively the memory won't leak).
  int (*instance_created)(int) = alloc_callback(&instance_closure, g_py_instance_created);
  g_py_instance_created = NULL;

  int (*instance_destroyed)(int) = alloc_callback(&instance_closure, g_py_instance_destroyed);
  g_py_instance_destroyed = NULL;

  core->RegisterInstanceCallbacks(extension,
      (XW_CreatedInstanceCallback) instance_created, (XW_CreatedInstanceCallback) instance_destroyed);

  core->RegisterShutdownCallback(extension, xw_handle_shutdown);

  g_xw_messaging = get_interface(XW_MESSAGING_INTERFACE);
  g_xw_messaging->Register(extension, xw_handle_message);

  g_xw_sync_messaging = get_interface(XW_INTERNAL_SYNC_MESSAGING_INTERFACE);
  g_xw_sync_messaging->Register(extension, xw_handle_sync_message);

  return XW_OK;
}
