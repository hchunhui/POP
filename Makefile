
SUBDIRS = io xswitch maple topo apps cbench main

all:
	@for d in ${SUBDIRS}; do \
		make -C $${d}; \
	done

clean:
	@for d in ${SUBDIRS}; do \
		make -C $${d} clean; \
	done

