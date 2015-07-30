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

#ifdef ENABLE_WEB
#include "web/ws.h"
#endif

extern const char *msg_get_pof_version(void);
extern void xswitch_init(const char *algo_file, const char *spec_file);
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
	fprintf(stderr, "%s [-t] [-v] [-p 6633] [-f apps/l3_multi.so] [-s scripts/header.spec]\n"
			"\t-t      Run as real-time thread\n"
			"\t-v      Verbose\n"
			"\t-p PORT Specify the server port\n"
			"\t-f FILE Specify algorithm\n"
			"\t-s FILE Specify header spec\n",
			prgname);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	const char *algo_file = "apps/l3_multi.so";
	const char *spec_file = "scripts/header.spec";
	int ch;

	while ((ch = getopt(argc, argv, "p:tvf:s:")) != -1) {
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
		case 'f':
			algo_file = optarg;
			break;
		case 's':
			spec_file = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}
	argc -= optind;
	argv += optind;

#ifdef ENABLE_WEB
	ws_init();
#endif
	fprintf(stderr, "POP Version: %s\n", VERSION);
	fprintf(stderr, "POF Version: %s\n", msg_get_pof_version());
	init_io_module();
	xswitch_init(algo_file, spec_file);

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
