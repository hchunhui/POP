#ifndef _IO_MSGQUEUE_H
#define _IO_MSGQUEUE_H

/* This file belongs to POP I/O module */

#include <pthread.h>
#include "io.h"

struct msgbuf;

struct msgqueue {
	pthread_mutex_t mtx;

	struct msgbuf *head;
	struct msgbuf *tail;
};

extern struct msgqueue recv_queue[NR_WORKERS];

void msgqueue_init(struct msgqueue *queue);
int msgqueue_enqueue(struct msgqueue *queue, struct msgbuf *p);
struct msgbuf *msgqueue_dequeue(struct msgqueue *queue);

#endif
