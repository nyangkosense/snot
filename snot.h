#ifndef SNOT_H
#define SNOT_H

#include <wayland-client.h>
#include <cairo/cairo.h>
#include <pango/pango.h>
#include "protocols/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "protocols/xdg-shell-client-protocol.h"
#include <stdbool.h> 

typedef struct {
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    uint32_t width, height;
    char *summary;
    char *body;
    char *app_name;
    int32_t replaces_id;
    uint32_t expire_timeout;
    unsigned long start_time;
    float opacity;
    cairo_surface_t *cairo_surface;
    cairo_t *cairo;
    bool configured;
} Notification;

void add_notification(const char *summary, const char *body,
                     const char *app_name, uint32_t replaces_id,
                     uint32_t expire_timeout);

#endif 