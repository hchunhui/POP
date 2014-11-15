TOPDIR = ..
PROG = controller
SRCS = main.c
EXTRA_OBJS = \
	${TOPDIR}/apps/libapps.a \
	${TOPDIR}/maple/libmaple.a ${TOPDIR}/topo/libtopo.a \
	${TOPDIR}/xswitch/libxswitch.a \
	${TOPDIR}/io/libmaple-io.a

EXTRA_CFLAGS = -O2 -fno-strict-aliasing -Wall -g -I/usr/local/include -I/home/btw/local/include
EXTRA_LDFLAGS = -L/home/btw/local/lib -L/usr/local/lib -lev

-include ${TOPDIR}/Makefile.comm
