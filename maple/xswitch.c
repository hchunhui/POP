#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xswitch-private.h"
#include "io/msgbuf.h"

#include "maple.h"
#include "topo.h"

#include "io/sw.h"

void xswitch_send(struct xswitch *sw, struct msgbuf *b)
{
	b->sw = sw->sw;
	send_msgbuf(b);
}

struct xswitch *xswitch_on_accept(struct sw *_sw)
//struct xswitch *xswitch_on_accept(struct vconn *vconn)
{
	struct xswitch *sw;
	struct msgbuf *msg;

	sw = malloc(sizeof *sw);
	sw->dpid = 0;
	//sw->rconn = rconn_new_from_vconn("tcp", vconn);
	sw->n_ports = 0;

	_sw->xsw = sw;
	sw->sw = _sw;

	fprintf(stderr, "sending hello\n");
	msg = msg_hello();
	xswitch_send(sw, msg);
	sw->state = XS_HELLO;
	return sw;
}

void xswitch_on_recv(struct xswitch *sw, struct msgbuf *msg)
{
	switch(sw->state) {
	case XS_HELLO:
		fprintf(stderr, "expect hello\n");
		if(msg_process_hello(msg)) {
			fprintf(stderr, "sending features request\n");
			msg = msg_features_request();
			xswitch_send(sw, msg);
			sw->state = XS_FEATURES_REPLY;
		}
		break;
	case XS_FEATURES_REPLY:
		fprintf(stderr, "expect features reply\n");
		if(msg_process_features_reply(msg, &sw->dpid, &sw->n_ports)) {
			struct msgbuf *msg;
			fprintf(stderr, "sending set/get config\n");
			msg = msg_set_config(128);
			xswitch_send(sw, msg);
			msg = msg_get_config_request();
			xswitch_send(sw, msg);
			xswitch_up(sw);
			sw->state = XS_RUNNING;
		}
		break;
	case XS_RUNNING:
		msg_process(sw, msg);
		break;
	default:
		fprintf(stderr, "error switch state.\n");
		abort();
	}
	msgbuf_delete(msg);
}

//void xswitch_on_close(struct xswitch *sw)
void xswitch_on_close(struct sw *_sw)
{
	struct xswitch *sw = _sw->xsw;
	if (sw->state == XS_RUNNING)
		xswitch_down(sw);
	//rconn_destroy(sw->rconn);
        free(sw);
}


//--- message handlers
void xswitch_up(struct xswitch *sw)
{
	topo_switch_up(sw);
	maple_switch_up(sw);
}

void xswitch_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len)
{
	int i;
	fprintf(stderr, "packet in, dpid: 0x%x, in_port: %u, total_len: %u\n",
		sw->dpid, in_port, packet_len);
	for(i = 0; i < packet_len; i++)
	{
		if(i%16 == 0)
			fprintf(stderr, "%06x ", i);
		fprintf(stderr, "%02x%c", packet[i], i%16==15 ? '\n' : ' ');
	}
	fprintf(stderr, "\n");
	topo_packet_in(sw, in_port, packet, packet_len);
	maple_packet_in(sw, in_port, packet, packet_len);
}

void xswitch_down(struct xswitch *sw)
{
	maple_switch_down(sw);
	topo_switch_down(sw);
}

#if 0
int xswitch_run(struct xswitch *sw)
{
	struct msgbuf *msg;

	int retval = 0;
	int retval2;

	msg = rconn_recv(sw->rconn);
	if(msg) {
		xswitch_on_recv(sw, msg);
		msgbuf_delete(msg);
	} else {
		retval = EAGAIN;
	}
	retval2 = rconn_run(sw->rconn);
	if(retval == 0 || retval2 == 0)
		return 0;
	else
		return retval2;
}

void xswitch_wait(struct xswitch *sw)
{
	rconn_run_wait(sw->rconn);
	rconn_recv_wait(sw->rconn);
}
#endif
