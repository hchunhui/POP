SUBDIRS = lib l2_learning l3_multi compress nat

all:
	@for d in ${SUBDIRS}; do \
		make -C $${d}; \
	done

clean:
	@for d in ${SUBDIRS}; do \
		make -C $${d} clean; \
	done

