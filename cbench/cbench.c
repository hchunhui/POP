/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 *
 * A dumb OpenFlow 1.0 responder for benchmarking the controller I/O engine.
 * Intended to be used with oflops cbench.
 *
 * This is intended to be comparable with pox cbench app
 *     https://github.com/noxrepo/pox/pox/misc/cbench.py
 * and ryu cbench app.
 *     https://github.com/osrg/ryu/ryu/app/cbench.py
 */

#include <stdio.h>

#include <netinet/in.h>

#include "io/msgbuf.h"
#include "io/sw.h"
#include "io/io.h"

#include "pof/pof_global.h"
#include "openflow/openflow.h"

void
accept_cb_func(struct sw *sw)
{
	struct msgbuf *msg = msgbuf_new(1024); // XXX
	struct pof_header *p = (struct pof_header *)msg->data;

	msg->sw = sw;

	p->version = 0x01;
	p->type = OFPT_FEATURES_REQUEST;
	p->length = htons(sizeof(*p));
	p->xid = 0;

	send_msgbuf(msg);

	printf("Connected.\n");
}

void
close_cb_func(struct sw *sw)
{
	printf("Closed.\n");
}

#ifdef RETURN_PACKET_OUT_PACKET
void
recv_cb_func(struct msgbuf *msg)
{
	struct pof_header *p = (struct pof_header *)msg->data;
	//struct sw *sw = msg->sw;

	if (p->type == OFPT_PACKET_IN) {
		p->type = OFPT_PACKET_OUT;
		send_msgbuf(msg);
	}
}
#else
void
recv_cb_func(struct msgbuf *msg)
{
	struct pof_header *p = (struct pof_header *)msg->data;
	struct sw *sw = msg->sw;

	if (p->type == OFPT_PACKET_IN) {
		struct msgbuf *mod = msgbuf_new(1024); // XXX
		mod->sw = sw;
		struct ofp_flow_mod *ofm = (struct ofp_flow_mod *)mod->data;

		/*
		 * Refer: ryu/ryu/ofproto/ofproto_v1_0_parser.py
		 */
		ofm->header.version = 0x01;
		ofm->header.type = OFPT_FLOW_MOD;
		ofm->header.length = htons(sizeof(*ofm));
		ofm->header.xid = 0; // 每次应该++

		//ofm->match. = xxx; // XXX

		ofm->cookie = 0;
		ofm->command = OFPFC_ADD;
		ofm->idle_timeout = 0;
		ofm->hard_timeout = 0;
		ofm->priority = OFP_DEFAULT_PRIORITY;
		ofm->buffer_id = 0xffffffff;
		ofm->out_port = OFPP_NONE;
		ofm->flags = 0;
		//ofm->actions array is 0 length.

		send_msgbuf(mod);
	}
	msgbuf_delete(msg);
}
#endif

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
