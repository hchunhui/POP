
SUBDIRS = io maple cbench maple

all:
	@for d in ${SUBDIRS}; do \
		make -C $${d}; \
	done

check-syntax:
	@for d in ${SUBDIRS}; do \
		make -C $${d} check-syntax; \
	done

clean:
	@for d in ${SUBDIRS}; do \
		make -C $${d} clean; \
	done

