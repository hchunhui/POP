TOPDIR = ..
PROG = pofmaple_pox.so
SRCS = main.c
EXTRA_OBJS = \
	${TOPDIR}/apps/libapps.a \
	${TOPDIR}/maple/libmaple.a ${TOPDIR}/topo/libtopo.a \
	${TOPDIR}/xswitch/libxswitch.a \
	${TOPDIR}/io/msgbuf.o

EXTRA_CFLAGS = -shared -O2 -fno-strict-aliasing -Wall -g -I/usr/include/python2.7
EXTRA_LDFLAGS =

-include ${TOPDIR}/Makefile.comm
