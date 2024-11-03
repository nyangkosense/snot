#ifndef DBUS_H
#define DBUS_H

#include <stdint.h>
#include <dbus/dbus.h>

#define SNOT_DBUS_INTERFACE "org.freedesktop.Notifications"
#define SNOT_DBUS_PATH "/org/freedesktop/Notifications"

int dbus_get_fd(void);
int dbus_init(void);
void dbus_destroy(void);
int dbus_dispatch(void);

#endif 