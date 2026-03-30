PC_DEPS := wayland-client wayland-egl glesv2 egl xkbcommon apr-1 sdl3-ttf
APR_EV := /work/libQ/apr_ev/apr_events.c -I/work/libQ/apr_ev/
SRCS := main.c qwindow.c xdg-shell-protocol.c qlayout_renderer.c

all: main.c qwindow.c
	gcc -o wayland_gl.app `pkg-config --cflags $(PC_DEPS)` $(SRCS) $(APR_EV) `pkg-config --libs $(PC_DEPS)` -lq3d
