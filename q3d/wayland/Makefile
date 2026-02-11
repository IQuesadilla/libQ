PC_DEPS := wayland-client wayland-egl glesv2 egl xkbcommon

all: main.c
	gcc -o wayland_gl.app `pkg-config --cflags $(PC_DEPS)` main.c xdg-shell-protocol.c `pkg-config --libs $(PC_DEPS)` -lq3d
