#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <signal.h>
#include <glib-unix.h>
#include <string.h>

#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_UUID "12345678-1234-5678-1234-56789abcdef1"
#define ROOT "/org/openlumi/bletts"
#define SERVICE ROOT "/service0"
#define CHAR SERVICE "/char0"

static GDBusConnection *bus;
static GMainLoop *loop;
static guint reg_ids[3];
static guint advertisement_id;
static GDBusNodeInfo *node;
static const gchar xml[] =
"<node>"
"<interface name='org.freedesktop.DBus.ObjectManager'>"
"<method name='GetManagedObjects'><arg type='a{oa{sa{sv}}}' direction='out'/></method>"
"</interface>"
"<interface name='org.bluez.GattService1'>"
"<property name='UUID' type='s' access='read'/>"
"<property name='Primary' type='b' access='read'/>"
"</interface>"
"<interface name='org.bluez.GattCharacteristic1'>"
"<method name='WriteValue'><arg type='ay' direction='in'/><arg type='a{sv}' direction='in'/></method>"
"<property name='UUID' type='s' access='read'/>"
"<property name='Service' type='o' access='read'/>"
"<property name='Flags' type='as' access='read'/>"
"</interface>"
"<interface name='org.bluez.LEAdvertisement1'>"
"<method name='Release'/>"
"<property name='Type' type='s' access='read'/>"
"<property name='ServiceUUIDs' type='as' access='read'/>"
"<property name='LocalName' type='s' access='read'/>"
"</interface>"
"</node>";

static GVariant *service_props(void) {
  GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&b, "{sv}", "UUID", g_variant_new_string(SERVICE_UUID));
  g_variant_builder_add(&b, "{sv}", "Primary", g_variant_new_boolean(TRUE));
  return g_variant_builder_end(&b);
}
static GVariant *char_props(void) {
  GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&b, "{sv}", "UUID", g_variant_new_string(CHAR_UUID));
  g_variant_builder_add(&b, "{sv}", "Service", g_variant_new_object_path(SERVICE));
  const gchar *flags[] = {"write", "write-without-response", NULL};
  g_variant_builder_add(&b, "{sv}", "Flags", g_variant_new_strv(flags, -1));
  return g_variant_builder_end(&b);
}
static void speak(const gchar *data, gsize len) {
  if (!len || len > 4096 || memchr(data, '\0', len)) return;
  gchar *text = g_strndup(data, len);
  if (!g_utf8_validate(text, -1, NULL)) { g_free(text); return; }
  gchar *argv[] = {"/usr/bin/espeak", "-v", "ro", "-a", "180", text, NULL};
  gchar **env = g_get_environ();
  env = g_environ_setenv(env, "AUDIODEV", "sysdefault:CARD=tfa9882audio", TRUE);
  GError *error = NULL;
  if (!g_spawn_async(NULL, argv, env, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
    g_printerr("eSpeak: %s\n", error->message); g_clear_error(&error);
  }
  g_strfreev(env); g_free(text);
}
static void method_call(GDBusConnection *c, const gchar *sender, const gchar *path,
                        const gchar *iface, const gchar *method, GVariant *params,
                        GDBusMethodInvocation *inv, gpointer user_data) {
  if (g_str_equal(iface, "org.freedesktop.DBus.ObjectManager") && g_str_equal(method, "GetManagedObjects")) {
    GVariantBuilder objects, interfaces;
    g_variant_builder_init(&objects, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    g_variant_builder_init(&interfaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&interfaces, "{s@a{sv}}", "org.bluez.GattService1", service_props());
    g_variant_builder_add(&objects, "{o@a{sa{sv}}}", SERVICE, g_variant_builder_end(&interfaces));
    g_variant_builder_init(&interfaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&interfaces, "{s@a{sv}}", "org.bluez.GattCharacteristic1", char_props());
    g_variant_builder_add(&objects, "{o@a{sa{sv}}}", CHAR, g_variant_builder_end(&interfaces));
    g_dbus_method_invocation_return_value(inv, g_variant_new("(@a{oa{sa{sv}}})", g_variant_builder_end(&objects)));
  } else if (g_str_equal(iface, "org.bluez.GattCharacteristic1") && g_str_equal(method, "WriteValue")) {
    GVariant *bytes, *options;
    g_variant_get(params, "(@ay@a{sv})", &bytes, &options);
    gsize n; const guint8 *p = g_variant_get_fixed_array(bytes, &n, sizeof(guint8));
    if (n == 0 || n > 4096 || memchr(p, 0, n) || !g_utf8_validate((const gchar *)p, n, NULL)) {
      g_dbus_method_invocation_return_dbus_error(inv, "org.bluez.Error.InvalidValueLength", "Invalid UTF-8 text or length");
    } else {
      speak((const gchar *)p, n);
      g_dbus_method_invocation_return_value(inv, NULL);
    }
    g_variant_unref(bytes); g_variant_unref(options);
  } else if (g_str_equal(iface, "org.bluez.LEAdvertisement1") && g_str_equal(method, "Release")) {
    g_dbus_method_invocation_return_value(inv, NULL);
    g_printerr("Advertisement released by BlueZ\n");
  } else {
    g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.UnknownMethod", "Unknown method");
  }
}
static GVariant *get_property(GDBusConnection *c, const gchar *sender, const gchar *path,
                              const gchar *iface, const gchar *property, GError **error, gpointer data) {
  if (g_str_equal(iface, "org.bluez.GattService1")) {
    if (g_str_equal(property, "UUID")) return g_variant_new_string(SERVICE_UUID);
    if (g_str_equal(property, "Primary")) return g_variant_new_boolean(TRUE);
  } else if (g_str_equal(iface, "org.bluez.GattCharacteristic1")) {
    if (g_str_equal(property, "UUID")) return g_variant_new_string(CHAR_UUID);
    if (g_str_equal(property, "Service")) return g_variant_new_object_path(SERVICE);
    if (g_str_equal(property, "Flags")) { const gchar *flags[] = {"write", "write-without-response", NULL}; return g_variant_new_strv(flags, -1); }
  } else if (g_str_equal(iface, "org.bluez.LEAdvertisement1")) {
    if (g_str_equal(property, "Type")) return g_variant_new_string("peripheral");
    if (g_str_equal(property, "ServiceUUIDs")) { const gchar *uuids[] = {SERVICE_UUID, NULL}; return g_variant_new_strv(uuids, -1); }
    if (g_str_equal(property, "LocalName")) return g_variant_new_string("OpenLumi-TTS");
  }
  g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Unknown property");
  return NULL;
}
static const GDBusInterfaceVTable vtable = {method_call, get_property, NULL, {0}};
static gboolean stop_loop(gpointer data) { g_main_loop_quit(loop); return G_SOURCE_REMOVE; }
static void on_signal(int sig) { g_idle_add(stop_loop, NULL); }

static void registered(GObject *source, GAsyncResult *res, gpointer data) {
  GError *error = NULL;
  GVariant *v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
  if (error) { g_printerr("Registration error: %s\n", error->message); g_clear_error(&error); }
  else { g_print("Registered: %s\n", (const char *)data); g_variant_unref(v); }
}
int main(void) {
  GError *error = NULL;
  bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus) { g_printerr("System D-Bus: %s\n", error->message); return 1; }
  node = g_dbus_node_info_new_for_xml(xml, &error);
  if (!node) { g_printerr("Introspection: %s\n", error->message); return 1; }
  const gchar *paths[] = {ROOT, SERVICE, CHAR, ROOT "/advertisement0"};
  const gchar *ifaces[] = {"org.freedesktop.DBus.ObjectManager", "org.bluez.GattService1", "org.bluez.GattCharacteristic1", "org.bluez.LEAdvertisement1"};
  for (int i=0;i<4;i++) {
    GDBusInterfaceInfo *info = g_dbus_node_info_lookup_interface(node, ifaces[i]);
    if (!g_dbus_connection_register_object(bus, paths[i], info, &vtable, NULL, NULL, &error)) {
      g_printerr("Register %s: %s\n", paths[i], error->message); return 1;
    }
  }
  loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add(SIGINT, stop_loop, NULL);
  g_unix_signal_add(SIGTERM, stop_loop, NULL);
  g_dbus_connection_call(bus, "org.bluez", "/org/bluez/hci0", "org.bluez.GattManager1", "RegisterApplication",
      g_variant_new("(o@a{sv})", ROOT, g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)), NULL, G_DBUS_CALL_FLAGS_NONE, 10000, NULL, registered, "GATT");
  g_dbus_connection_call(bus, "org.bluez", "/org/bluez/hci0", "org.bluez.LEAdvertisingManager1", "RegisterAdvertisement",
      g_variant_new("(o@a{sv})", ROOT "/advertisement0", g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)), NULL, G_DBUS_CALL_FLAGS_NONE, 10000, NULL, registered, "advertising");
  g_main_loop_run(loop);
  return 0;
}
