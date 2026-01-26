all: libqbuild.so install qb qbinstall

libqbuild.so: qbuild.c qbuild.h test/build.c
	gcc -O2 qbuild.c -g -c -fPIC -o /tmp/qbuild.o `pkg-config apr-1 --cflags`
	gcc -shared -g -o libqbuild.so /tmp/qbuild.o `pkg-config apr-1 --libs`

qb: libqbuild.so qb.c
	gcc qb.c -o qb -g -lqbuild `pkg-config apr-1 --cflags --libs`

install: libqbuild.so
	sudo mkdir -p /usr/local/lib
	sudo mkdir -p /usr/local/include
	sudo cp libqbuild.so /usr/local/lib/
	sudo cp qbuild.h /usr/local/include/
	sudo cp qb.h /usr/local/include/

qbinstall: qb
	sudo mkdir -p /usr/local/bin
	sudo cp qb /usr/local/bin/
