
LIB := libmaple-io.a
OBJS := core.o util.o msgqueue.o msgbuf.o

CFLAGS := -O2 -fno-strict-aliasing -Wall -I /home/btw/local/include -I ../include -g

all: ${LIB}

${LIB}: ${OBJS}
	ar rcs ${LIB} ${OBJS}

clean:
	rm -f ${LIB} *.o
