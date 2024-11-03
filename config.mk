VERSION = 0.1

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

WLROOTS_DIR ?= $(shell pkg-config --variable=prefix wlroots 2>/dev/null)

PKG_CONFIG = pkg-config

INCS = $(shell ${PKG_CONFIG} --cflags pixman-1) \
       $(shell ${PKG_CONFIG} --cflags libdrm) \
       $(shell ${PKG_CONFIG} --cflags pango) \
       $(shell ${PKG_CONFIG} --cflags pangocairo) \
       $(shell ${PKG_CONFIG} --cflags cairo) \
       $(shell ${PKG_CONFIG} --cflags dbus-1)

ifeq ($(WLROOTS_DIR),)
    INCS += $(shell ${PKG_CONFIG} --cflags wlroots)
else
    INCS += -I$(WLROOTS_DIR)/include
endif

LIBS = $(shell ${PKG_CONFIG} --libs wayland-client) \
       $(shell ${PKG_CONFIG} --libs pangocairo) \
       $(shell ${PKG_CONFIG} --libs cairo) \
       $(shell ${PKG_CONFIG} --libs dbus-1)

ifeq ($(WLROOTS_DIR),)
    LIBS += $(shell ${PKG_CONFIG} --libs wlroots)
else
    LIBS += -L$(WLROOTS_DIR)/lib -lwlroots
endif

CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS = -O2 -Wall ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

CC = gcc