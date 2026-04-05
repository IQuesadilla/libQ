TARGET := libqwayland.so
PC_DEPS := apr-1 wayland-client wayland-cursor wayland-egl egl xkbcommon sdl3 sdl3-ttf

WL_SRCS := xdg-shell-protocol.c fractional-scale-v1-protocol.c viewporter-protocol.c
SRCS := qwindow.c qcam.c qlayout_renderer.c /work/libQ/apr_ev/apr_events.c
HDRS := qwindow.h qcam.h qlayout_renderer.h /work/libQ/apr_ev/apr_events.h

INC_DIRS := -g -I/work/libQ/apr_ev -I/work/libQ/deps
CFLAGS := $(INC_DIRS) `pkg-config --cflags $(PC_DEPS) glesv2`
LIBS := `pkg-config --libs $(PC_DEPS)`

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	gcc -shared -fPIC -O2 -o $(TARGET) $(CFLAGS) $(SRCS) $(WL_SRCS) $(LIBS)

install: $(TARGET) $(HDRS)
	cp $(TARGET) /work/root/lib
	cp qcam.h /work/root/include

client: $(WL_SRCS)

xdg-shell-client-protocol.h: /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
	wayland-scanner client-header \
  	/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  	xdg-shell-client-protocol.h

xdg-shell-protocol.c: xdg-shell-client-protocol.h
	wayland-scanner private-code \
  	/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  	xdg-shell-protocol.c

fractional-scale-v1-client-protocol.h: /usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml
	wayland-scanner client-header \
  	/usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml \
  	fractional-scale-v1-client-protocol.h

fractional-scale-v1-protocol.c: fractional-scale-v1-client-protocol.h
	wayland-scanner private-code \
  	/usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml \
  	fractional-scale-v1-protocol.c

viewporter-client-protocol.h: /usr/share/wayland-protocols/stable/viewporter/viewporter.xml
	wayland-scanner client-header \
  	/usr/share/wayland-protocols/stable/viewporter/viewporter.xml \
  	viewporter-client-protocol.h

viewporter-protocol.c: viewporter-client-protocol.h
	wayland-scanner private-code \
  	/usr/share/wayland-protocols/stable/viewporter/viewporter.xml \
  	viewporter-protocol.c
