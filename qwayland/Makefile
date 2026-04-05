TARGET := libqwayland.so
PC_DEPS := apr-1 wayland-client wayland-cursor wayland-egl egl xkbcommon

DEPS := /work/libQ/deps/deps.a
SRCS := qwindow.c qcam.c qlayout_renderer.c /work/libQ/apr_ev/apr_events.c
HDRS := qwindow.h qcam.h qlayout_renderer.h /work/libQ/apr_ev/apr_events.h

INC_DIRS := -g -I/work/libQ/apr_ev -I/work/libQ/deps
CFLAGS := $(INC_DIRS) `pkg-config --cflags $(PC_DEPS) glesv2 sdl3 sdl3-ttf`
LIBS := `pkg-config --libs $(PC_DEPS)`

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	gcc -shared -fPIC -O2 -o $(TARGET) $(CFLAGS) $(SRCS) $(DEPS) $(LIBS)

install: $(TARGET) $(HDRS)
	cp $(TARGET) /work/root/lib
	cp qcam.h /work/root/include
