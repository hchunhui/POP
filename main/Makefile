TOPDIR = ..
PROG = controller
SRCS = main.c
EXTRA_OBJS = \
	${TOPDIR}/maple/libmaple.a ${TOPDIR}/topo/libtopo.a \
	${TOPDIR}/xswitch/libxswitch.a \
	${TOPDIR}/io/libmaple-io.a
VERSION != ${TOPDIR}/scripts/get_version

EXTRA_CFLAGS = -D'VERSION="${VERSION}"' -O2 -Wall -g -I/usr/local/include -I/home/btw/local/include
EXTRA_LDFLAGS = -Wl,--export-dynamic -L/home/btw/local/lib -L/usr/local/lib -lev -ldl

-include ${TOPDIR}/Makefile.comm
