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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ev.h>

#include "util.h"
#include "smp.h"

#include "io.h"
#include "sw.h"
#include "msgqueue.h"
#include "msgbuf.h"

#include "pof/pof_global.h"

//#define TEST_LOAD_BALANCE

static pthread_t io_thread[NR_IO_THREADS];
static int sockfd = 0;

static int server_port = 6633;

extern void accept_cb_func(struct sw *sw);
extern void close_cb_func(struct sw *sw);

static void
txrx_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	struct sw *sw = w->data;
	int cpuid = sw->cpuid;

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

		msg = msgbuf_new(1024); // XXX
		assert(msg != NULL);

		ret = recv_packet(w->fd, msg);
		if (ret == -1) {
			msgbuf_delete(msg);
			return;
		}
		if (ret == 0) {
			/* Handle the closed fds */
			msgbuf_delete(msg);
			close(w->fd);
			ev_io_stop(loop, w);
			close_cb_func(sw);
			free(w);
			return;
		}
		msg->sw = w->data;
		//printf("length1: %d\n", ((struct pof_header *)msg->data)->length);

#if 0
		if (enqueue(&recv_queue[cpuid], msg) != 0) {
			fprintf(stderr, "failed to enqueue a packet.\n");
			msgbuf_delete(msg);
		}
#else
		void recv_cb_func(struct msgbuf *msg);
		recv_cb_func(msg); // XXX 谁free这个msg。
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
			assert(ret != -1);
			assert(ret == ntohs(((struct pof_header *)msg->data)->length));
			msgbuf_delete(msg);
		}
	}
}

struct accept_ctx {
	int cpuid;
#ifdef TEST_LOAD_BALANCE
	int count;
#endif
};

static void
accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	int newfd;
	ev_io *new_watcher;
	struct sockaddr_in sin;
	socklen_t addrlen;
	struct accept_ctx *ctx = w->data;
	int cpuid = ctx->cpuid;
	struct sw *sw;

	if (!(revents & EV_READ))
		return;

#ifdef TEST_LOAD_BALANCE
	ctx->count++;

	if (ctx->count % 100 == 0)
		printf("pthread %d: %d\n", ctx->cpuid, ctx->count);
#endif

	addrlen = sizeof(sin);
	newfd = accept(w->fd, (struct sockaddr *)&sin, &addrlen);
	if (newfd < 0) {
		fprintf(stderr, "Failed to accept(): %s\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	adjust_bufsize(newfd);
	make_fd_nonblock(newfd);

	new_watcher = malloc(sizeof(*new_watcher));
	assert(new_watcher != NULL);

	sw = new_sw(cpuid);
	new_watcher->data = sw;

	accept_cb_func(sw);

	ev_io_init(new_watcher, txrx_cb, newfd, EV_READ|EV_WRITE);
	ev_io_start(loop, new_watcher);

#ifdef DEBUG
	static int accept_count = 0;
	printf("accpet %d connections\n", ++accept_count);
#endif
}

static void *
routine(void *arg)
{
	int cpuid = (int)(intptr_t)arg;
	struct ev_loop *loop;
	ev_io listen_watcher;

	//bind_cpu(cpuid);

	loop = ev_loop_new(EVFLAG_AUTO);

	ev_io_init(&listen_watcher, accept_cb, sockfd, EV_READ);
	struct accept_ctx *ctx = malloc(sizeof(*ctx));
	assert(ctx != NULL);
	ctx->cpuid = cpuid;
#ifdef TEST_LOAD_BALANCE
	ctx->count = 0;
#endif
	listen_watcher.data = ctx;
	ev_io_start(loop, &listen_watcher);

	ev_run(loop, 0);

	return (NULL);
}

static int
create_server(void)
{
	int fd;
	struct sockaddr_in sin;
	int yes = 1;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		perror("setsockopt(SO_REUSEADDR)");
		exit(EXIT_FAILURE);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(server_port);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if (listen(fd, 5) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	return (fd);
}

static void
init_server(void)
{
	int i;

	for (i = 0; i < NR_IO_THREADS; i++)
		msgqueue_init(&recv_queue[i]);
}

int
init_io_module(void)
{
	int i;

	sockfd = create_server();
	init_server();

	for (i = 0; i < NR_IO_THREADS; i++) {
		if (pthread_create(&io_thread[i], NULL, routine,
				   (void *)(intptr_t)i) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	printf("I/O engine is up.\n");

	return (0);
}

void
fini_io_module(void)
{
	int i;

	for (i = 0; i < NR_IO_THREADS; i++) {
		if (pthread_join(io_thread[i], NULL) != 0) {
			perror("pthread_join");
			exit(EXIT_FAILURE);
		}
	}
}
