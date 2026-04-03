PC_DEPS := glesv2 apr-1 apr-util-1 serf-1 lua
APR_EV := /work/libQ/apr_ev/apr_events.c -I/work/libQ/apr_ev/
SRCS := main.c lua_clay.c downloader.c
HDRS := lua_clay.h downloader.h
INC_DIR := -I/work/libQ/qwayland/
CFLAGS := -g $(INC_DIR) `pkg-config --cflags $(PC_DEPS)`
LIBS := -lm `pkg-config --libs $(PC_DEPS)` -L/work/libQ/qwayland/ -lqwayland

all: qbrowser

qbrowser: $(SRCS) $(HDRS) /work/libQ/qwayland/libqwayland.so
	gcc -O2 -o qbrowser $(CFLAGS) $(SRCS) $(APR_EV) $(LIBS)
