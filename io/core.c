/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <err.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ev.h>

#include "param.h"

#include "util.h"
#include "thread.h"

#include "io_int.h"
#include "io.h"
#include "sw.h"
#include "msgqueue.h"
#include "msgbuf.h"

#include "pof/pof_global.h"

static struct master master;
static struct worker workers[NR_WORKERS] __attribute__((aligned(64)));
static int sockfd = 0;

int server_port = 6633;
int async_send = 1;

extern void accept_cb_func(struct sw *sw);
extern void close_cb_func(struct sw *sw);
extern void timeout_cb_func(void);

static void
rxtx_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	struct sw *sw = w->data;

	/*
	 * XXX: 一定要使用: revents，而不是 w->events!
	 */
	if (revents & EV_READ) {
		/*
		 * 1. Read a packet
		 * 2. Setup a msgbuf
		 * 3. Append it to recv_queue
		 */
		struct msgbuf *msg;
		int ret;

		msg = msgbuf_new(2048); // XXX
		assert(msg != NULL);

		ret = recv_packet(w->fd, msg);
		if (ret == -1) {
			msgbuf_delete(msg);
			return;
		}
		if (ret == 0) {
			/* Handle the closed fds */
			msgbuf_delete(msg);
			goto closed;
		}
		msg->sw = w->data;
#ifdef DEBUG
		printf("length1: %d\n", ((struct pof_header *)msg->data)->length);
#endif

#if 0
		if (enqueue(&recv_queue[cpuid], msg) != 0) {
			fprintf(stderr, "failed to enqueue a packet.\n");
			msgbuf_delete(msg);
		}
#else
		void recv_cb_func(struct msgbuf *msg);
		recv_cb_func(msg);
#endif
	}

	if (revents & EV_WRITE) {
		/*
		 * Send all msgs saved in w->data->send_queue.
		 */
		struct sw *sw = w->data;
		struct msgbuf *msg;

		while ((msg = msgqueue_dequeue(&sw->send_queue)) != NULL) {
			int ret = write(w->fd, msg->data, ntohs(
			      ((struct pof_header *)msg->data)->length));
			if (ret == -1 && (errno == EPIPE ||
					  errno == ECONNRESET)) {
				/* Handle the closed fds */
				msgbuf_delete(msg);
				goto closed;
			}
			if (ret == -1) {
				// XXX Handle failed writes
				printf("errno: %d %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
			assert(ret == ntohs(((struct pof_header *)msg->data)->length));
			msgbuf_delete(msg);
		}

		if (async_send) {
			sw->async_pending = 0;
			ev_io_stop(loop, w);
			w->events = EV_READ;
			ev_io_start(loop, w);
		}
	}

	return;

closed:
	close_cb_func(sw);
	close(w->fd);

	ev_io_stop(loop, w);
	if (async_send)
		ev_async_stop(loop, sw->async_watcher);

	free(w);
	if (async_send)
		free(sw->async_watcher);
	free(sw);
}

static void
cq_init(struct cq_head *cq)
{
	pthread_mutex_init(&cq->lock, NULL);
	cq->head = NULL;
	cq->tail = NULL;
}

static struct cq_item *
cq_pop(struct cq_head *cq)
{
	struct cq_item *item;

	pthread_mutex_lock(&cq->lock);
	item = cq->head;
	if (item != NULL) {
		cq->head = item->next;
		if (cq->head == NULL)
			cq->tail = NULL;
	}
	pthread_mutex_unlock(&cq->lock);

	return (item);
}

static void
cq_push(struct cq_head *cq, struct cq_item *item)
{
	item->next = NULL;

	pthread_mutex_lock(&cq->lock);
	if (cq->head == NULL)
		cq->head = item;
	else
		cq->tail->next = item;
	cq->tail = item;
	pthread_mutex_unlock(&cq->lock);
}

static void
dispatch_conn(int fd)
{
	static int round_robin = 0;
	struct cq_item *item = malloc(sizeof(*item));
	struct worker *worker = &workers[round_robin++ % NR_WORKERS];

	assert(item != NULL);
	item->fd = fd;

	cq_push(&worker->new_conn_queue, item);
	ev_async_send(worker->loop, &worker->async_watcher);
}

static void
accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int newfd;
	struct sockaddr_in sin;
	socklen_t addrlen = sizeof(sin);

	assert(revents & EV_READ);

	newfd = accept(w->fd, (struct sockaddr *)&sin, &addrlen);
	if (newfd < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return;
		fprintf(stderr, "Failed to accept(): %s\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	//adjust_bufsize(newfd);
	//make_fd_nonblock(newfd);
	make_fd_block(newfd);

	dispatch_conn(newfd);
}

static void
async_send_cb(struct ev_loop *loop, ev_async *w, int revents)
{
	ev_io *rxtx_watcher = w->data;

	ev_io_stop(loop, rxtx_watcher);
	rxtx_watcher->events |= EV_WRITE;
	ev_io_start(loop, rxtx_watcher);
}

static void
async_accept_cb(struct ev_loop *loop, ev_async *w, int revents)
{
	struct worker *me = w->data;
	ev_io *rxtx_watcher;
	struct ev_async *async_watcher;
	struct sw *sw;
	struct cq_item *item;

again:
	item = cq_pop(&me->new_conn_queue);
	if (item == NULL)
		return;

	sw = new_sw(me->cpuid);
	sw->worker = me;

	rxtx_watcher = malloc(sizeof(*rxtx_watcher));
	assert(rxtx_watcher != NULL);
	rxtx_watcher->data = sw;

	if (async_send) {
		async_watcher = malloc(sizeof(*async_watcher));
		assert(async_watcher != NULL);
		async_watcher->data = rxtx_watcher;

		sw->async_watcher = async_watcher;
	}

	ev_io_init(rxtx_watcher, rxtx_cb, item->fd, EV_READ|EV_WRITE);
	ev_io_start(loop, rxtx_watcher);

	if (async_send) {
		ev_async_init(async_watcher, async_send_cb);
		ev_async_start(loop, async_watcher);
	}

	free(item);
	accept_cb_func(sw);

	goto again;
}

static void
timeout_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
	timeout_cb_func();
}

static void *
worker_thread(void *arg)
{
	struct worker *me = arg;
	char thread_name[32];

	sprintf(thread_name, "worker %d", me->cpuid);
	thread_setname(thread_name);

	bind_cpu(me->cpuid);

	ev_loop(me->loop, 0);
	return (NULL);
}

static void
setup_worker(struct worker *me)
{
	me->loop = ev_loop_new(0);
	assert(me->loop != NULL);

	cq_init(&me->new_conn_queue);

	me->async_watcher.data = me;
	ev_async_init(&me->async_watcher, async_accept_cb);
	ev_async_start(me->loop, &me->async_watcher);

	ev_timer_init(&me->timeout_watcher, timeout_cb, TIMEOUT_INTERVAL,
		      TIMEOUT_INTERVAL);
	ev_timer_start(me->loop, &me->timeout_watcher);
}

static void *
master_thread(void *arg)
{
	struct master *me = arg;
	char thread_name[32];

	sprintf(thread_name, "master");
	thread_setname(thread_name);

	bind_cpu(NR_WORKERS);

	ev_loop(me->loop, 0);

	return (NULL);
}

static void
setup_master(struct master *me)
{
	me->loop = ev_loop_new(0);
	assert(me->loop != NULL);

	me->listen_watcher.data = me;
	ev_io_init(&me->listen_watcher, accept_cb, sockfd, EV_READ);
	ev_io_start(me->loop, &me->listen_watcher);
}

static int
create_server(void)
{
	int fd;
	struct sockaddr_in sin;
	int yes = 1;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		err(EXIT_FAILURE, "socket");

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		err(EXIT_FAILURE, "setsockopt(SO_REUSEADDR)");

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(server_port);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(EXIT_FAILURE, "bind");

	if (listen(fd, 100000) == -1)
		err(EXIT_FAILURE, "listen");

	make_fd_nonblock(fd);

	return (fd);
}

static void
init_server(void)
{
	int i;

	for (i = 0; i < NR_WORKERS; i++)
		msgqueue_init(&recv_queue[i]);
}

static void
init_thread(void)
{
	pthread_attr_t attr;
	struct sched_param param;
	int i;

	thread_setname("main routine");

	if (pthread_attr_init(&attr) != 0)
		err(EXIT_FAILURE, "pthread_attr_init");

	if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0)
		err(EXIT_FAILURE, "pthread_attr_setinheritsched");

	if (realtime) {
		if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0)
			err(EXIT_FAILURE, "pthread_attr_setschedpolicy");

		param.sched_priority = 1;
		if (pthread_attr_setschedparam(&attr, &param) != 0)
			err(EXIT_FAILURE, "pthread_attr_setschedparam");
	}

	setup_master(&master);
	if (pthread_create(&master.pthread, &attr, master_thread,
			   &master) != 0)
		err(EXIT_FAILURE, "pthread_create");

	for (i = 0; i < NR_WORKERS; i++) {
		workers[i].cpuid = i;
		setup_worker(&workers[i]);
	}

	for (i = 0; i < NR_WORKERS; i++) {
		if (pthread_create(&workers[i].pthread, &attr, worker_thread,
				   &workers[i]) != 0)
			err(EXIT_FAILURE, "pthread_create");
	}
}

int
init_io_module(void)
{
	signal(SIGPIPE, SIG_IGN);

	sockfd = create_server();
	init_server();
	init_thread();

	if (verbose)
		printf("I/O engine is up.\n");

	return (0);
}

void
fini_io_module(void)
{
	int i;

	if (pthread_join(master.pthread, NULL) != 0)
		err(EXIT_FAILURE, "pthread_join");

	for (i = 0; i < NR_WORKERS; i++) {
		if (pthread_join(workers[i].pthread, NULL) != 0)
			err(EXIT_FAILURE, "pthread_join");
	}
}

