
LIB := libmaple-io.a
OBJS := core.o util.o msgqueue.o msgbuf.o

CFLAGS := -O2 -fno-strict-aliasing -Wall -g \
          -I../include -I/home/btw/local/include -I/usr/local/include

all: ${LIB}

${LIB}: ${OBJS}
	ar rcs ${LIB} ${OBJS}

clean:
	rm -f ${LIB} *.o
