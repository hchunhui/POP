#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xswitch-private.h"
#include "io/msgbuf.h"

#include "maple/maple.h"
#include "topo/topo.h"

void xswitch_init(const char *algo_file, const char *spec_file)
{
	maple_init(algo_file, spec_file);
}

void xswitch_send(struct xswitch *sw, struct msgbuf *b)
{
	b->sw = sw->conn;
	send_msgbuf(b);
}

dpid_t xswitch_get_dpid(struct xswitch *sw)
{
	return sw->dpid;
}

int xswitch_get_num_ports(struct xswitch *sw)
{
	return sw->n_ports;
}

struct xswitch *xswitch(dpid_t dpid, int ports, void *conn)
{
	struct xswitch *sw;
	struct msgbuf *msg;

	sw = malloc(sizeof *sw);
	sw->dpid = dpid;
	sw->n_ports = sw->n_ready_ports = ports;
	sw->conn = conn;

	if(dpid) {
		sw->state = XS_RUNNING;
		xswitch_up(sw);
	} else {
		sw->state = XS_HELLO;
		msg = msg_hello();
		xswitch_send(sw, msg);
	}
	return sw;
}

void xswitch_free(struct xswitch *sw)
{
	if (sw->state == XS_RUNNING)
		xswitch_down(sw);
        free(sw);
}

struct xswitch *xswitch_on_accept(void *conn)
{
	return xswitch(0, 0, conn);
}

void xswitch_on_recv(struct xswitch *sw, struct msgbuf *msg)
{
	struct msgbuf *rmsg;
	switch(sw->state) {
	case XS_HELLO:
		fprintf(stderr, "expect hello\n");
		if(msg_process_hello(msg)) {
			fprintf(stderr, "sending features request\n");
			rmsg = msg_features_request();
			xswitch_send(sw, rmsg);
			sw->state = XS_FEATURES_REPLY;
		}
		break;
	case XS_FEATURES_REPLY:
		fprintf(stderr, "expect features reply\n");
		if(msg_process_features_reply(msg, &sw->dpid, &sw->n_ports)) {
			fprintf(stderr, "sending set/get config\n");
			rmsg = msg_set_config(128);
			xswitch_send(sw, rmsg);
			rmsg = msg_get_config_request();
			xswitch_send(sw, rmsg);
			sw->state = XS_RUNNING;
			/*
			 * Note:
			 * We can not call xswitch_up() here,
			 * because ports are not ready.
			 */
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

void xswitch_on_close(struct xswitch *sw)
{
	xswitch_free(sw);
}

static void init_table0(struct xswitch *sw)
{
	struct msgbuf *msg;
	struct match *ma;
	struct action *ac;
	/* init match fields */
	sw->table0 = flow_table(0, FLOW_TABLE_TYPE_MM, 1024);
	flow_table_add_field(sw->table0, "in_port", MATCH_FIELD_METADATA, 16, 8);
	flow_table_add_field(sw->table0, "dl_dst", MATCH_FIELD_PACKET, 0, 48);
	flow_table_add_field(sw->table0, "dl_src", MATCH_FIELD_PACKET, 48, 48);
	flow_table_add_field(sw->table0, "dl_type", MATCH_FIELD_PACKET, 96, 16);
	/* match_field 不够用了，暂时不要nw_proto */
	/* flow_table_add_field(sw->table0, "nw_proto", MATCH_FIELD_PACKET, 112+64+8, 8); */
	flow_table_add_field(sw->table0, "nw_src", MATCH_FIELD_PACKET, 112+96, 32);
	flow_table_add_field(sw->table0, "nw_dst", MATCH_FIELD_PACKET, 112+128, 32);
	flow_table_add_field(sw->table0, "tp_src", MATCH_FIELD_PACKET, 112+160, 16);
	flow_table_add_field(sw->table0, "tp_dst", MATCH_FIELD_PACKET, 112+176, 16);

	/* create table */
	msg = msg_flow_table_add(sw->table0);
	xswitch_send(sw, msg);

	/* init entry */
	ma = match();
	ac = action();
	action_add(ac, AC_PACKET_IN, 0);
	msg = msg_flow_entry_add(sw->table0, flow_table_get_entry_index(sw->table0), 0, ma, ac);
	match_free(ma);
	action_free(ac);
	xswitch_send(sw, msg);
}

//--- message handlers
void xswitch_up(struct xswitch *sw)
{
	init_table0(sw);
	sw->next_table_id = 1;
	maple_switch_up(sw);
	topo_switch_up(sw);
}

void xswitch_packet_in(struct xswitch *sw, int in_port, uint8_t *packet, int packet_len)
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
	if (!topo_packet_in(sw, in_port, packet, packet_len))
		maple_packet_in(sw, in_port, packet, packet_len);
}

void xswitch_port_status(struct xswitch *sw, int port, enum port_status status)
{
	topo_switch_port_status(sw, port, status);
}

void xswitch_down(struct xswitch *sw)
{
	topo_switch_down(sw);
	maple_switch_down(sw);
	flow_table_free(sw->table0);
}

void xswitch_on_timeout(void)
{
	static int seconds = 0;
	printf("%d seconds past\n", seconds += 5);
}
