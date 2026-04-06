PC_DEPS := glesv2 apr-1 apr-util-1 lua serf-1
APR_EV := /work/libQ/apr_ev/apr_events.c -I/work/libQ/apr_ev/
SRCS := main.c lua_clay.c downloader.c
HDRS := lua_clay.h downloader.h
INC_DIR := -I/work/libQ/qwayland/ -I/work/libQ/deps
CFLAGS := -g $(INC_DIR) `pkg-config --cflags $(PC_DEPS)`
LIBS := -lm `pkg-config --libs $(PC_DEPS)` -L/work/libQ/qwayland/ -lqwayland
# Weirdly, specifying libserf-1.so.0 forces libserf-1.so.1, fixing everything

all: qbrowser

qbrowser: $(SRCS) $(HDRS) /work/libQ/qwayland/libqwayland.so
	gcc -O2 -o qbrowser $(CFLAGS) $(SRCS) $(APR_EV) $(LIBS)
