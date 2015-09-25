
SUBDIRS = xlog io xswitch core topo web apps cbench main

all:
	@for d in ${SUBDIRS}; do \
		make -C $${d}; \
	done

clean:
	@for d in ${SUBDIRS}; do \
		make -C $${d} clean; \
	done

dist:
	scripts/dist

apps_dist:
	scripts/apps_dist
