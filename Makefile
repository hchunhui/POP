
SUBDIRS = io maple cbench maple

all:
	@for d in ${SUBDIRS}; do \
		make -C $${d}; \
	done

clean:
	@for d in ${SUBDIRS}; do \
		make -C $${d} clean; \
	done

