include config.mk

SRCS = snot.c dbus.c \
       protocols/wlr-layer-shell-unstable-v1-protocol.c \
       protocols/xdg-shell-protocol.c

OBJS = $(SRCS:.c=.o)

WAYLAND_SCANNER = wayland-scanner
PROTO_DIR = protocols

LAYER_XML = $(PROTO_DIR)/wlr-layer-shell-unstable-v1.xml
LAYER_HEADER = $(PROTO_DIR)/wlr-layer-shell-unstable-v1-client-protocol.h
LAYER_CODE = $(PROTO_DIR)/wlr-layer-shell-unstable-v1-protocol.c

XDG_XML = $(PROTO_DIR)/xdg-shell.xml
XDG_HEADER = $(PROTO_DIR)/xdg-shell-client-protocol.h
XDG_CODE = $(PROTO_DIR)/xdg-shell-protocol.c

all: snot

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LAYER_CODE): $(LAYER_XML)
	@mkdir -p $(PROTO_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(LAYER_HEADER): $(LAYER_XML)
	@mkdir -p $(PROTO_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

$(XDG_CODE): $(XDG_XML)
	@mkdir -p $(PROTO_DIR)
	$(WAYLAND_SCANNER) private-code $< $@

$(XDG_HEADER): $(XDG_XML)
	@mkdir -p $(PROTO_DIR)
	$(WAYLAND_SCANNER) client-header $< $@

protocols/wlr-layer-shell-unstable-v1-protocol.o: $(LAYER_CODE) $(LAYER_HEADER) $(XDG_HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

protocols/xdg-shell-protocol.o: $(XDG_CODE) $(XDG_HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

snot.o: snot.c $(LAYER_HEADER) $(XDG_HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

dbus.o: dbus.c
	$(CC) $(CFLAGS) -c $< -o $@

snot: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f snot $(OBJS) $(PROTO_DIR)/*-protocol.* $(PROTO_DIR)/*-client-protocol.*

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f snot $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/snot
	mkdir -p $(DESTDIR)$(PREFIX)/share/snot
	cp -f config.def.h $(DESTDIR)$(PREFIX)/share/snot/config.def.h
	chmod 644 $(DESTDIR)$(PREFIX)/share/snot/config.def.h

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/snot
	rm -rf $(DESTDIR)$(PREFIX)/share/snot

.PHONY: all clean install uninstall