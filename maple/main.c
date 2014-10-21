#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>

#include "poll-loop.h"
#include "timeval.h"
#include "util.h"
#include "vconn.h"
#include "xswitch.h"

#define MAX_SWITCHES 4096

struct xswitch;

int main(int argc, char *argv[])
{
	struct xswitch *switches[MAX_SWITCHES];
	struct pvconn *pvconn;
	int n_switches = 0;
	int retval;

	set_program_name(argv[0]);
	time_init();
	signal(SIGPIPE, SIG_IGN);

	retval = pvconn_open("ptcp:", &pvconn);
	if(retval) {
		fprintf(stderr, "connect: %s\n", strerror(retval));
		ofp_fatal(0, "connection");
	}

	while (1) {
		struct vconn *new_vconn;
		int iteration;
		int i;

		/* Accept connections on pvconn. */
		if (n_switches < MAX_SWITCHES) {
			retval = pvconn_accept(pvconn, 0, &new_vconn);
			if (!retval || retval == EAGAIN) {
				if (!retval) {
					fprintf(stderr, "switch up\n");
					switches[n_switches] = xswitch_on_accept(new_vconn);
					n_switches++;
				}
			} else {
				break;
			}
		}

		/* Do some switching work.  Limit the number of iterations so that
		 * callbacks registered with the poll loop don't starve. */
		for (iteration = 0; iteration < 50; iteration++) {
			bool progress = false;
			for (i = 0; i < n_switches; ) {
				struct xswitch *sw = switches[i];
				int retval = xswitch_run(sw);
				if (!retval || retval == EAGAIN) {
					if (!retval) {
						progress = true;
					}
					i++;
				} else {
					fprintf(stderr, "switch down\n");
					xswitch_on_close(sw);
					switches[i] = switches[--n_switches];
				}
			}
			if (!progress) {
				break;
			}
		}

		/* Wait for something to happen. */
		if (n_switches < MAX_SWITCHES) {
			pvconn_wait(pvconn);
		}

		for (i = 0; i < n_switches; i++) {
			struct xswitch *sw = switches[i];
			xswitch_wait(sw);
		}
		poll_block();
	}

	return 0;
}
