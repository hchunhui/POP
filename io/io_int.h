#ifndef _IO_IO_INT_H
#define _IO_IO_INT_H

#include <pthread.h>
#include <ev.h>

/* This file belongs to POP I/O module */

struct cq_item {
	int fd;
	struct cq_item *next;
};

struct cq_head {
	struct cq_item *head;
	struct cq_item *tail;
	pthread_mutex_t lock;
};

struct master {
	pthread_t pthread;
	struct ev_loop *loop;
	ev_io listen_watcher; /* io watcher for new connections */
};

struct worker {
	pthread_t pthread;
	int cpuid;
	struct ev_loop *loop;
	struct ev_async async_watcher; /* async watcher for new connections */
	struct ev_timer timeout_watcher;
	struct cq_head new_conn_queue;
};

#endif
