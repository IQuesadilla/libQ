TARGET := libqwayland.so
PC_DEPS := apr-1 wayland-client wayland-egl egl xkbcommon sdl3 sdl3-ttf
SRCS := qwindow.c qcam.c xdg-shell-protocol.c qlayout_renderer.c /work/libQ/apr_ev/apr_events.c
HDRS := qwindow.h qcam.h xdg-shell-client-protocol.h qlayout_renderer.h /work/libQ/apr_ev/apr_events.h
INC_DIRS := -g -I/work/libQ/apr_ev
CFLAGS := $(INC_DIRS) `pkg-config --cflags $(PC_DEPS) glesv2`
LIBS := `pkg-config --libs $(PC_DEPS)`

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	gcc -shared -fPIC -O2 -o $(TARGET) $(CFLAGS) $(SRCS) $(LIBS)

install: $(TARGET) $(HDRS)
	cp $(TARGET) /work/root/lib
	cp qcam.h /work/root/include
