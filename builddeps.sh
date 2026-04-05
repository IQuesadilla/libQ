cd deps/

mkdir -p cglm-src/build
cd cglm-src/build
cmake .. -DCGLM_STATIC=ON
make -j`nproc`
cd ../..
cp -r cglm-src/include/cglm .

gcc -c -fPIC \
  `pkg-config --cflags wayland-client` \
  xdg-shell-protocol.c \
  viewporter-protocol.c \
  fractional-scale-v1-protocol.c \
  `pkg-config --libs wayland-client`

ar rcs deps.a \
  xdg-shell-protocol.o \
  viewporter-protocol.o \
  fractional-scale-v1-protocol.o \
  cglm-src/build/libcglm.a
