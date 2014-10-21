/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 */

#include <stdio.h>
#include "io.h"
#include "sw.h"
#include "msgbuf.h"
#include "msgqueue.h"

struct msgqueue recv_queue[NR_IO_THREADS] __attribute__((aligned(64)));

void
msgqueue_init(struct msgqueue *queue)
{
	queue->head = NULL;
	queue->tail = NULL;

	pthread_mutex_init(&queue->mtx, NULL);
}

int
msgqueue_enqueue(struct msgqueue *queue, struct msgbuf *packet)
{
	pthread_mutex_lock(&queue->mtx);

	if (queue->head == NULL) {
		queue->head = packet;
		queue->tail = packet;
		packet->next = NULL;
	} else {
		queue->tail->next = packet;
		packet->next = NULL;
		queue->tail = packet;
	}

	pthread_mutex_unlock(&queue->mtx);

	return (0);
}

struct msgbuf *
msgqueue_dequeue(struct msgqueue *queue)
{
	struct msgbuf *packet;

	pthread_mutex_lock(&queue->mtx);

	packet = queue->head;
	if (packet != NULL) {
		queue->head = packet->next;
		if (queue->head == NULL)
			queue->tail = NULL;
	}

	pthread_mutex_unlock(&queue->mtx);

	return (packet);
}

/* ------------------------------------------------------------------------ */

struct msgbuf *
recv_msgbuf(int cpuid)
{
	return (msgqueue_dequeue(&recv_queue[cpuid]));
}

int
send_msgbuf(struct msgbuf *msg)
{
	return (msgqueue_enqueue(&msg->sw->send_queue, msg));
}

/* ------------------------------------------------------------------------ */
