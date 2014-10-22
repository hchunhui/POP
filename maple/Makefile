
LIB = libmaple-core.a
OBJS = xswitch.o flow_table.o match.o action.o \
       msg.o maple.o topo.o entity.o f.o

CFLAGS = -I.. -I../include

all: ${LIB}

${LIB}: ${OBJS}
	ar rcs ${LIB} ${OBJS}

clean:
	rm -f ${OBJS} ${LIB}
