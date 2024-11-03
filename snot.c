#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/mman.h>  
#include <wayland-client.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>  
#include "protocols/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "config.h"
#include "snot.h"
#include "dbus.h"
#include <fcntl.h>  
#include <sys/stat.h>  

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct zwlr_layer_shell_v1 *layer_shell;
static struct wl_shm *shm;

static Notification notifications[MAX_NOTIFICATIONS];
static int notification_count = 0;

static void draw_notification(Notification *n);
static void remove_notification(int index);

static void
die(const char *msg) {
    fprintf(stderr, "notifyd: %s\n", msg);
    exit(1);
}

static void
registry_global(void *data, struct wl_registry *registry,
                uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name,
                                    &wl_compositor_interface, 4);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell = wl_registry_bind(registry, name,
                                     &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name,
                             &wl_shm_interface, 1);
    }
}

static void
registry_global_remove(void *data, struct wl_registry *registry,
                      uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};


static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
                       uint32_t serial, uint32_t width, uint32_t height) {
    Notification *n = data;
    
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    
    n->configured = true;
    
    n->width = width > 0 ? width : NOTIFICATION_WIDTH;
    n->height = height > 0 ? height : NOTIFICATION_HEIGHT;

    printf("Surface configured: %dx%d\n", n->width, n->height);
    
    draw_notification(n);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
    Notification *n = data;
    int index = n - notifications;  
    if (index >= 0 && index < notification_count) {
        remove_notification(index);
    }
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void
create_notification_surface(Notification *n) {

    n->surface = NULL;
    n->layer_surface = NULL;
    n->cairo = NULL;
    n->cairo_surface = NULL;
    n->configured = false;

    n->surface = wl_compositor_create_surface(compositor);
    if (!n->surface) {
        fprintf(stderr, "Failed to create surface\n");
        return;
    }

    n->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, n->surface, NULL,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "notification");
    
    if (!n->layer_surface) {
        fprintf(stderr, "Failed to create layer surface\n");
        wl_surface_destroy(n->surface);
        return;
    }

    zwlr_layer_surface_v1_add_listener(n->layer_surface,
                                     &layer_surface_listener, n);

    zwlr_layer_surface_v1_set_size(n->layer_surface,
                                  NOTIFICATION_WIDTH,
                                  NOTIFICATION_HEIGHT);

    uint32_t anchor = 0;
    int margin_top = 0;
    int margin_right = 0;
    int margin_bottom = 0;
    int margin_left = 0;

    if (POSITION == 0) {  
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
        margin_top = SPACING;
    } else {  
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        margin_bottom = SPACING;
    }

    switch (ALIGNMENT) {
        case 0:  
            anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
            margin_left = SPACING;
            break;
        case 1:
            anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
            break;
        case 2:  
            anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
            margin_right = SPACING;
            break;
    }

    int stack_offset = notification_count * (NOTIFICATION_HEIGHT + SPACING);
    if (POSITION == 0) {  
        margin_top += stack_offset;
    } else {  
        margin_bottom += stack_offset;
    }

    zwlr_layer_surface_v1_set_anchor(n->layer_surface, anchor);
    zwlr_layer_surface_v1_set_margin(n->layer_surface,
                                    margin_top,
                                    margin_right,
                                    margin_bottom,
                                    margin_left);

    zwlr_layer_surface_v1_set_exclusive_zone(n->layer_surface, -1);

    n->cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                NOTIFICATION_WIDTH,
                                                NOTIFICATION_HEIGHT);
    if (cairo_surface_status(n->cairo_surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create Cairo surface\n");
        zwlr_layer_surface_v1_destroy(n->layer_surface);
        wl_surface_destroy(n->surface);
        return;
    }

    n->cairo = cairo_create(n->cairo_surface);
    if (cairo_status(n->cairo) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create Cairo context\n");
        cairo_surface_destroy(n->cairo_surface);
        zwlr_layer_surface_v1_destroy(n->layer_surface);
        wl_surface_destroy(n->surface);
        return;
    }

    wl_surface_commit(n->surface);

    printf("Notification surface created: pos=%s align=%s offset=%d\n",
           POSITION == 0 ? "top" : "bottom",
           ALIGNMENT == 0 ? "left" : (ALIGNMENT == 1 ? "center" : "right"),
           stack_offset);
}

static void
draw_notification(Notification *n) {
    if (!n->configured) {
        printf("Skipping draw, surface not configured yet\n");
        return;
    }

    cairo_t *cr = n->cairo;
    unsigned int r, g, b;

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    sscanf(BACKGROUND_COLOR, "#%02x%02x%02x", &r, &g, &b);
    cairo_set_source_rgba(cr, r/255.0, g/255.0, b/255.0, 0.9);
    cairo_rectangle(cr, 0, 0, n->width, n->height);
    cairo_fill(cr);

    sscanf(BORDER_COLOR, "#%02x%02x%02x", &r, &g, &b);
    cairo_set_source_rgb(cr, r/255.0, g/255.0, b/255.0);
    cairo_set_line_width(cr, BORDER_WIDTH);
    cairo_rectangle(cr, BORDER_WIDTH/2.0, BORDER_WIDTH/2.0,
                   n->width - BORDER_WIDTH, n->height - BORDER_WIDTH);
    cairo_stroke(cr);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc = pango_font_description_from_string(FONT);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    sscanf(FOREGROUND_COLOR, "#%02x%02x%02x", &r, &g, &b);
    cairo_set_source_rgb(cr, r/255.0, g/255.0, b/255.0);

    pango_layout_set_text(layout, n->summary, -1);
    cairo_move_to(cr, PADDING, PADDING);
    pango_cairo_show_layout(cr, layout);

    if (n->body) {
        pango_layout_set_text(layout, n->body, -1);
        cairo_move_to(cr, PADDING, PADDING * 2 + 15);  
        pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, n->width);
    int size = stride * n->height;
    
    char tmp[] = "/tmp/notifyd-XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) {
        fprintf(stderr, "Failed to create temporary file\n");
        return;
    }
    unlink(tmp);

    if (ftruncate(fd, size) < 0) {
        fprintf(stderr, "Failed to set file size\n");
        close(fd);
        return;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap\n");
        close(fd);
        return;
    }

    memcpy(data, cairo_image_surface_get_data(n->cairo_surface), size);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
                                                        n->width, n->height,
                                                        stride,
                                                        WL_SHM_FORMAT_ARGB8888);
    
    wl_shm_pool_destroy(pool);
    close(fd);
    
    wl_surface_attach(n->surface, buffer, 0, 0);
    wl_surface_damage_buffer(n->surface, 0, 0, n->width, n->height);
    wl_surface_commit(n->surface);

    wl_buffer_destroy(buffer);
    munmap(data, size);
}

void
add_notification(const char *summary, const char *body,
                const char *app_name, uint32_t replaces_id,
                uint32_t expire_timeout) {
    Notification *n;

    if (replaces_id > 0) {
        for (int i = 0; i < notification_count; i++) {
            if (notifications[i].replaces_id == replaces_id) {
                n = &notifications[i];
                free(n->summary);
                free(n->body);
                free(n->app_name);
                goto replace;
            }
        }
    }

    if (notification_count >= MAX_NOTIFICATIONS)
        return;
    n = &notifications[notification_count++];
    create_notification_surface(n);

replace:
    n->summary = strdup(summary);
    n->body = body ? strdup(body) : NULL;
    n->app_name = strdup(app_name);
    n->replaces_id = replaces_id;
    n->expire_timeout = expire_timeout;
    n->start_time = time(NULL) * 1000;
    n->opacity = 1.0;

    draw_notification(n);
}

static void
remove_notification(int index) {
    if (index < 0 || index >= notification_count)
        return;

    Notification *n = &notifications[index];

    if (n->layer_surface) {
        zwlr_layer_surface_v1_destroy(n->layer_surface);
        n->layer_surface = NULL;
    }

    if (n->surface) {
        wl_surface_destroy(n->surface);
        n->surface = NULL;
    }

    if (n->cairo) {
        cairo_destroy(n->cairo);
        n->cairo = NULL;
    }

    if (n->cairo_surface) {
        cairo_surface_destroy(n->cairo_surface);
        n->cairo_surface = NULL;
    }

    free(n->summary);
    free(n->body);
    free(n->app_name);

    for (int i = index; i < notification_count - 1; i++) {
        notifications[i] = notifications[i + 1];
    }

    notification_count--;

    for (int i = 0; i < notification_count; i++) {
        if (notifications[i].configured && notifications[i].layer_surface) {
            int stack_offset = i * (NOTIFICATION_HEIGHT + SPACING);
            if (POSITION == 0) {  
                zwlr_layer_surface_v1_set_margin(notifications[i].layer_surface,
                                               SPACING + stack_offset,
                                               ALIGNMENT == 2 ? SPACING : 0,
                                               0,
                                               ALIGNMENT == 0 ? SPACING : 0);
            } else {  
                zwlr_layer_surface_v1_set_margin(notifications[i].layer_surface,
                                               0,
                                               ALIGNMENT == 2 ? SPACING : 0,
                                               SPACING + stack_offset,
                                               ALIGNMENT == 0 ? SPACING : 0);
            }
            wl_surface_commit(notifications[i].surface);
        }
    }
}

int
main(void) {

    display = wl_display_connect(NULL);
    if (!display)
        die("Cannot connect to Wayland display");

    printf("Connected to Wayland display\n");

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !layer_shell || !shm)
        die("Missing required Wayland protocols");

    printf("Wayland protocols initialized\n");

    if (dbus_init() < 0)
        die("Failed to initialize D-Bus");

    printf("D-Bus initialized\n");

while (1) {

    while (wl_display_prepare_read(display) != 0) {
        wl_display_dispatch_pending(display);
    }
    wl_display_flush(display);

    int wayland_fd = wl_display_get_fd(display);
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(wayland_fd, &read_fds);

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 16666  
    };

    int ret = select(wayland_fd + 1, &read_fds, NULL, NULL, &tv);

    if (ret < 0 && errno != EINTR) {
        fprintf(stderr, "select failed: %s\n", strerror(errno));
        break;
    }

    if (ret > 0 && FD_ISSET(wayland_fd, &read_fds)) {
        if (wl_display_read_events(display) < 0) {
            fprintf(stderr, "Failed to read Wayland events\n");
            break;
        }
    } else {
        wl_display_cancel_read(display);
    }

    if (wl_display_dispatch_pending(display) < 0) {
        fprintf(stderr, "Failed to dispatch Wayland events\n");
        break;
    }

    if (dbus_dispatch() < 0) {
        fprintf(stderr, "Failed to dispatch D-Bus events\n");
        break;
    }

    unsigned long current_time = time(NULL) * 1000;
    for (int i = 0; i < notification_count; i++) {
        Notification *n = &notifications[i];
        if (!n->configured)
            continue;

        unsigned long display_time = (n->expire_timeout == -1) ? 
                                  DURATION : n->expire_timeout;

        if (display_time > 0 && 
            current_time - n->start_time >= display_time) {
            printf("Removing expired notification %d\n", i);
            remove_notification(i--);
        }
    }
}}