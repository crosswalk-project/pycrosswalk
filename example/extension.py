import datetime
import gi.repository
import xwalk

from gi.repository import GLib


def Timeout(instance, message):
  now = str(datetime.datetime.now())
  reply = "Hello from python: %d %s %s" % (instance, message, now)

  xwalk.PostMessage(instance, reply)

  # XXX: If we don't push a reference here, we get an error
  # and the main loop doesn't fire again.
  GLib.MainContext.ref_thread_default()

  return True


def HandleMessage(instance, message):
  GLib.timeout_add_seconds(2, Timeout, instance, message)


def HandleSyncMessage(instance, message):
  now = str(datetime.datetime.now())

  return "Hello from python: %d %s %s" % (instance, message, now)


def HandleInstanceCreated(instance):
  xwalk.SetMessageCallback(instance, HandleMessage)
  xwalk.SetSyncMessageCallback(instance, HandleSyncMessage)


def HandleInstanceDestroyed(instance):
  return


def Main():
  xwalk.SetExtensionName("example")
  xwalk.SetInstanceCreatedCallback(HandleInstanceCreated)
  xwalk.SetInstanceDestroyedCallback(HandleInstanceDestroyed)
  xwalk.SetJavaScriptAPI(
    "var listener = null;"
    "extension.setMessageListener(function(msg) {"
    "  if (listener instanceof Function) {"
    "    listener(msg);"
    "  };"
    "});"
    "exports.startTimeTick = function(msg, callback) {"
    "  listener = callback;"
    "  extension.postMessage(msg);"
    "};"
    "exports.getTimeSync = function(msg) {"
    "  return extension.internal.sendSyncMessage(msg);"
    "};")

Main()
