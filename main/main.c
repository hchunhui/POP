/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 */

#include <stdio.h>

#include <netinet/in.h>

#include "io/msgbuf.h"
#include "io/io.h"
#include "io/sw.h"

#include "pof/pof_global.h"

extern struct xswitch *xswitch_on_accept(struct sw *_sw);
extern void xswitch_on_recv(struct xswitch *sw, struct msgbuf *msg);
extern void xswitch_on_close(struct sw *_sw);

void
accept_cb_func(struct sw *sw)
{
	xswitch_on_accept(sw);
	printf("Connected.\n");
}

void
close_cb_func(struct sw *sw)
{
	xswitch_on_close(sw);
	printf("Closed.\n");
}

void
recv_cb_func(struct msgbuf *msg)
{
	xswitch_on_recv(msg->sw->xsw, msg);
	//msgbuf_delete(msg); // XXX 谁来释放？
}

int
main(void)
{
	init_io_module();

#if 0
	while (1) {
		struct msgbuf *msg;

		msg = recv_msgbuf(0);
		if (msg != NULL) {
			recv_cb_func(msg);
		}
	}
#else
	while (1)
		;
#endif

	fini_io_module();

	return (0);
}
