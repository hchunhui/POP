TOPDIR = ..
PROG = pofmaple_pox.so
SRCS = main.c
EXTRA_OBJS = \
	${TOPDIR}/maple/libmaple.a ${TOPDIR}/topo/libtopo.a \
	${TOPDIR}/xswitch/libxswitch.a \
	${TOPDIR}/io/msgbuf.o

EXTRA_CFLAGS = -shared -O2 -Wall -g -I/usr/include/python2.7
EXTRA_LDFLAGS = -lpython2.7 -ldl -Wl,--export-dynamic

-include ${TOPDIR}/Makefile.comm
