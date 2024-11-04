#define SNOT_DBUS_INTERFACE "org.freedesktop.Notifications"
#define SNOT_DBUS_PATH "/org/freedesktop/Notifications"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include "dbus.h"
#include "snot.h"
#include "config.h"


static DBusConnection *connection;
static uint32_t next_notification_id = 1;

/* https://dbus.freedesktop.org/doc/dbus-api-design.html */
static const char introspection_xml[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.freedesktop.Notifications\">\n"
    "    <method name=\"Notify\">\n"
    "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"replaces_id\" type=\"u\" direction=\"in\"/>\n"
    "      <arg name=\"app_icon\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"summary\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"body\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"actions\" type=\"as\" direction=\"in\"/>\n"
    "      <arg name=\"hints\" type=\"a{sv}\" direction=\"in\"/>\n"
    "      <arg name=\"expire_timeout\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"id\" type=\"u\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetServerInformation\">\n"
    "      <arg name=\"name\" type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"vendor\" type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"version\" type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"spec_version\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetCapabilities\">\n"
    "      <arg name=\"capabilities\" type=\"as\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

int
dbus_get_fd(void) {
    int fd;
    dbus_connection_get_unix_process_id(connection, &fd);
    return fd;
}

static DBusHandlerResult
method_get_server_information(DBusConnection *conn, DBusMessage *msg) {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    const char *name = "snot";
    const char *vendor = "snot";
    const char *version = "1.0";
    const char *spec_version = "1.2";

    dbus_message_append_args(reply,
                           DBUS_TYPE_STRING, &name,
                           DBUS_TYPE_STRING, &vendor,
                           DBUS_TYPE_STRING, &version,
                           DBUS_TYPE_STRING, &spec_version,
                           DBUS_TYPE_INVALID);

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
method_get_capabilities(DBusConnection *conn, DBusMessage *msg) {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;

    DBusMessageIter iter, array;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);

    const char *capabilities[] = {
        "body",
        "body-markup",
        "actions",
        NULL
    };

    for (const char **cap = capabilities; *cap; cap++) {
        dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, cap);
    }

    dbus_message_iter_close_container(&iter, &array);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_notification_method(DBusConnection *conn, DBusMessage *msg) {
    DBusMessageIter iter;
    char *app_name = NULL, *app_icon = NULL, *summary = NULL, *body = NULL;
    uint32_t replaces_id = 0;
    int32_t expire_timeout = -1;

    printf("Received notification request\n");

    if (!dbus_message_iter_init(msg, &iter)) {
        fprintf(stderr, "Message has no arguments\n");
        goto error;
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
        fprintf(stderr, "First argument is not a string\n");
        goto error;
    }
    dbus_message_iter_get_basic(&iter, &app_name);
    printf("App name: %s\n", app_name);
    if (!dbus_message_iter_next(&iter)) goto error;

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_UINT32) goto error;
    dbus_message_iter_get_basic(&iter, &replaces_id);
    printf("Replaces ID: %u\n", replaces_id);
    if (!dbus_message_iter_next(&iter)) goto error;

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) goto error;
    dbus_message_iter_get_basic(&iter, &app_icon);
    printf("App icon: %s\n", app_icon);
    if (!dbus_message_iter_next(&iter)) goto error;

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) goto error;
    dbus_message_iter_get_basic(&iter, &summary);
    printf("Summary: %s\n", summary);
    if (!dbus_message_iter_next(&iter)) goto error;

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) goto error;
    dbus_message_iter_get_basic(&iter, &body);
    printf("Body: %s\n", body);

    if (!dbus_message_iter_next(&iter)) goto error;
    if (!dbus_message_iter_next(&iter)) goto error;

    if (!dbus_message_iter_next(&iter)) goto error;
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INT32) goto error;
    dbus_message_iter_get_basic(&iter, &expire_timeout);

    printf("Creating notification...\n");
    add_notification(summary, body, app_name, replaces_id, expire_timeout);
    printf("Notification created\n");

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (reply) {
        uint32_t id = next_notification_id++;
        dbus_message_append_args(reply,
                               DBUS_TYPE_UINT32, &id,
                               DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        printf("Reply sent, ID: %u\n", id);
    }

    return DBUS_HANDLER_RESULT_HANDLED;

error:
    fprintf(stderr, "Failed to parse notification message\n");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
handle_message(DBusConnection *conn, DBusMessage *msg, void *user_data) {
    printf("Received D-Bus message\n");  // Debug print

    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        printf("Handling Introspect request\n");  // Debug print
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (!reply) {
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }

        dbus_message_append_args(reply,
                               DBUS_TYPE_STRING, &introspection_xml,
                               DBUS_TYPE_INVALID);

        dbus_connection_send(conn, reply, NULL);
        dbus_connection_flush(conn);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(msg, SNOT_DBUS_INTERFACE, "GetServerInformation")) {
        printf("Handling GetServerInformation request\n");  // Debug print
        return method_get_server_information(conn, msg);
    }

    if (dbus_message_is_method_call(msg, SNOT_DBUS_INTERFACE, "GetCapabilities")) {
        printf("Handling GetCapabilities request\n");  // Debug print
        return method_get_capabilities(conn, msg);
    }

    if (dbus_message_is_method_call(msg, SNOT_DBUS_INTERFACE, "Notify")) {
        printf("Handling Notify request\n");  // Debug print
        DBusHandlerResult result = handle_notification_method(conn, msg);
        dbus_connection_flush(conn);
        return result;
    }

    printf("Unhandled D-Bus message\n");  // Debug print
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable vtable = {
    .message_function = handle_message,
    .unregister_function = NULL,
};

int
dbus_init(void) {
    DBusError err;
    dbus_error_init(&err);

    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Failed to connect to bus: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }

    dbus_connection_set_exit_on_disconnect(connection, FALSE);

    int ret = dbus_bus_request_name(connection, SNOT_DBUS_INTERFACE,
                                  DBUS_NAME_FLAG_REPLACE_EXISTING,
                                  &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Failed to request name: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "Not primary owner of interface\n");
        return -1;
    }

    if (!dbus_connection_register_object_path(connection, SNOT_DBUS_PATH,
                                            &vtable, NULL)) {
        fprintf(stderr, "Failed to register object path\n");
        return -1;
    }

    printf("D-Bus initialized successfully\n");
    return 0;
}

void
dbus_destroy(void) {
    if (connection) {
        dbus_connection_unref(connection);
    }
}

int
dbus_dispatch(void) {
    dbus_connection_read_write_dispatch(connection, 0);
    return 0;
}