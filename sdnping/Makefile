
CFLAGS = -g -Wall -O2

all: sdnpingd sdnping

sdnpingd: sdnpingd.c
	gcc $(CFLAGS) $^ -o $@ -lpcap

sdnping: sdnping.c
	gcc $(CFLAGS) $^ -o $@ -lpcap

sdnsend: sdnsend.c
	gcc $(CFLAGS) $^ -o $@ -lpcap

clean:
	rm -rf sdnping sdnping.dSYM sdnpingd sdnping.dSYM sdnsend
