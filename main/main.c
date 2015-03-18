/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 */

#include <stdio.h>
#include <unistd.h>

#include <netinet/in.h>

#include "param.h"

#include "io/msgbuf.h"
#include "io/io.h"
#include "io/sw.h"

extern void xswitch_init(void);
extern struct xswitch *xswitch_on_accept(void *conn);
extern void xswitch_on_recv(struct xswitch *sw, struct msgbuf *msg);
extern void xswitch_on_close(struct xswitch *sw);
extern void xswitch_on_timeout(void);

int realtime = 0;
int verbose  = 0;

void
accept_cb_func(struct sw *sw)
{
	sw->xsw = xswitch_on_accept(sw);
	printf("Connected.\n");
}

void
close_cb_func(struct sw *sw)
{
	xswitch_on_close(sw->xsw);
	printf("Closed.\n");
}

void
recv_cb_func(struct msgbuf *msg)
{
	struct sw *sw = msg->sw;
	xswitch_on_recv(sw->xsw, msg);
}

void
timeout_cb_func(void)
{
	xswitch_on_timeout();
}

static void
usage(const char *prgname)
{
	fprintf(stderr, "%s [-t] [-v] [-p 6633]"
			"\t-t      Run as real-time thread\n"
			"\t-v      Verbose\n"
			"\t-p PORT Specify the server port\n",
			prgname);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "p:tv")) != -1) {
		switch (ch) {
		case 't':
			realtime = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'p':
			sscanf(optarg, "%d", &server_port);
			break;
		default:
			usage(argv[0]);
		}
	}
	argc -= optind;
	argv += optind;

	fprintf(stderr, "POF Version: %s\n", pof_version);
	init_io_module();
	xswitch_init();

#if 0
	while (1) {
		struct msgbuf *msg;

		msg = recv_msgbuf(0);
		if (msg != NULL) {
			recv_cb_func(msg);
		}
	}
#else
	pause();
#endif

	fini_io_module();

	return (0);
}
