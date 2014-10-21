
PROG := controller
OBJS := main.o

CFLAGS := -O2 -fno-strict-aliasing -Wall -I/home/btw/local/include -I.. -I../include -I../io -I../maple -g
LDFLAGS := -L/home/btw/local/lib -L../io -L../maple -lmaple-core -lmaple-io \
           -lev -lpthread

all: ${PROG}

${PROG}: ${OBJS}
	cc ${OBJS} -o ${PROG} ${CFLAGS} ${LDFLAGS}

clean:
	rm -f ${PROG} *.o
