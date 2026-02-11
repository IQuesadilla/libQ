all: libqbuild.so install qb qbinstall

libqbuild.so: qbuild.c qbuild.h test/build.c
	gcc -O2 qbuild.c -g -c -fPIC -o /tmp/qbuild.o `pkg-config apr-1 --cflags`
	gcc -O2 qexec.c -g -c -fPIC -o /tmp/qexec.o `pkg-config apr-1 --cflags`
	gcc -shared -g -o libqbuild.so /tmp/qbuild.o /tmp/qexec.o `pkg-config apr-1 --libs`

qb: libqbuild.so qb.c
	gcc qb.c -o qb -g -lqbuild -L/work/root/lib/ `pkg-config apr-1 --cflags --libs`

install: libqbuild.so
	cp libqbuild.so /work/root/lib/
	cp qbuild.h /work/root/include/
	cp qtest.h /work/root/include/
	cp qb.h /work/root/include/

qbinstall: qb
	cp qb /work/root/bin/
