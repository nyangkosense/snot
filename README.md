snot - suckless notification daemon
==================================

snot is a simple notification daemon for Wayland compositors implementing the 
layer-shell protocol.

Requirements
-----------
* wayland
* wlroots
* cairo
* pango
* dbus

Installation
-----------
Edit config.mk to match your local setup (snot is installed into
/usr/local by default).

Afterwards enter the following command to build and install snot
(if necessary as root):

    make clean install

Configuration
------------
The configuration of snot is done by creating a custom config.h
and (re)compiling the source code.

    cp config.def.h config.h
    $EDITOR config.h
    make clean snot