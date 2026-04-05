mkdir -p deps
cd deps/

wget https://github.com/nicbarker/clay/releases/download/v0.14/clay.h
wget https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image.h
wget https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_truetype.h

wget https://github.com/recp/cglm/archive/refs/tags/v0.9.6.zip
unzip v0.9.6.zip
rm v0.9.6.zip
mv cglm-0.9.6 cglm-src

wayland-scanner client-header \
  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  xdg-shell-client-protocol.h

wayland-scanner private-code \
  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  xdg-shell-protocol.c

wayland-scanner client-header \
  /usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml \
  fractional-scale-v1-client-protocol.h

wayland-scanner private-code \
  /usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml \
  fractional-scale-v1-protocol.c

wayland-scanner client-header \
  /usr/share/wayland-protocols/stable/viewporter/viewporter.xml \
  viewporter-client-protocol.h

wayland-scanner private-code \
  /usr/share/wayland-protocols/stable/viewporter/viewporter.xml \
  viewporter-protocol.c
