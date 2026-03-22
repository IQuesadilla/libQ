PC_DEPS := wayland-client wayland-egl glesv2 egl xkbcommon apr-1
APR_EV := /work/libQ/apr_ev/apr_events.c -I/work/libQ/apr_ev/

all: main.c qwindow.c
	gcc -o wayland_gl.app `pkg-config --cflags $(PC_DEPS)` main.c qwindow.c xdg-shell-protocol.c $(APR_EV) `pkg-config --libs $(PC_DEPS)` -lq3d
