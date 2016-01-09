#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xlog/xlog.h"
#include "xswitch-private.h"
#include "xswitch.h"
#include "io/msgbuf.h"

#include "core/core.h"
#include "topo/topo.h"

#include "io/sw.h"

#define XSWITCHMAX 100
static __thread int xswitchnum;
static __thread struct xswitch *xswitches[XSWITCHMAX];

void xswitch_init(const char *algo_file, const char *spec_file)
{
	core_init(algo_file, spec_file);
	topo_init();
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

struct xport **xswitch_get_xports(struct xswitch *sw)
{
	return sw->xports;
}

struct flow_table *xswitch_get_table0(struct xswitch *sw)
{
	return sw->table0;
}

void xswitch_table_lock(struct xswitch *sw)
{
	pthread_mutex_lock(&sw->table_lock);
}

void xswitch_table_unlock(struct xswitch *sw)
{
	pthread_mutex_unlock(&sw->table_lock);
}

struct xswitch *xswitch(dpid_t dpid, int ports, void *conn)
{
	struct xswitch *sw;
	struct msgbuf *msg;

	sw = malloc(sizeof *sw);
	sw->dpid = dpid;
	sw->n_ports = sw->n_ready_ports = ports;
	memset(sw->xports, 0, XPORT_HASH_SIZE * sizeof(struct xport *));
	sw->conn = conn;
	pthread_mutex_init(&sw->table_lock, NULL);

	if(dpid) {
		sw->state = XS_RUNNING;
	} else {
		sw->state = XS_HELLO;
		msg = msg_hello();
		xswitch_send(sw, msg);
	}

	xswitches[xswitchnum ++] = sw;
	assert(xswitchnum <= XSWITCHMAX);
	return sw;
}

void xswitch_free(struct xswitch *sw)
{
	int i;
	for (i = 0; i < xswitchnum; i++) {
		if (xswitches[i] == sw) {
			xswitches[i] = xswitches[-- xswitchnum];
			xswitches[xswitchnum] = NULL;
			break;
		}
	}

	if (sw->state == XS_RUNNING)
		xswitch_down(sw);
	pthread_mutex_destroy(&sw->table_lock);
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
		xdebug("expect hello\n");
		if(msg_process_hello(msg)) {
			xdebug("sending features request\n");
			rmsg = msg_features_request();
			xswitch_send(sw, rmsg);
			sw->state = XS_FEATURES_REPLY;
		}
		break;
	case XS_FEATURES_REPLY:
		xdebug("expect features reply\n");
		if(msg_process_features_reply(msg, &sw->dpid, &sw->n_ports)) {
			xdebug("sending set/get config\n");
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
		xerror("error switch state.\n");
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
	sw->table0 = flow_table(1, FLOW_TABLE_TYPE_MM, 100);
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

static void init_counter(struct xswitch *sw, int counter_id)
{
	struct msgbuf *msg;
	msg = msg_counter_add(counter_id);
	xdebug("add counter %d\n", counter_id);
	xswitch_send(sw, msg);
}

static void init_counter_table(struct xswitch *sw)
{
	struct msgbuf *msg;
	struct match *ma;
	struct action *ac;
	struct xport **xps, *xp;
	int i;
	uint16_t port_id;
	int priority = 1;
	int idx = 0;
	struct flow_table *ft = flow_table(0, FLOW_TABLE_TYPE_MM, 100);
	flow_table_add_field(ft, "in_port", MATCH_FIELD_METADATA, 16, 8);

	msg = msg_flow_table_add(ft);
	xswitch_send(sw, msg);

	xps = xswitch_get_xports(sw);
	for (i = 0; i < XPORT_HASH_SIZE; i++) {
		xp = xps[i];
		if (xp == NULL)
			continue;
next:
		port_id = xport_get_port_id(xp);
		init_counter(sw, port_id);
		/* match port_id */
		ma = match();
		match_add(ma, "in_port", value_from_8(port_id),
			  value_from_8(0xff));
		ac = action();
		//action_add(ac, AC_COUNTER, port_id);
		action_add_goto_table(ac, 1, 0);
		msg = msg_flow_entry_add(ft, idx++, priority++, ma, ac);
		match_free(ma);
		action_free(ac);
		xdebug("add counter %u on port %u\n", port_id, port_id);
		xswitch_send(sw, msg);
		if ((xp = xport_get_next(xp)) != NULL)
			goto next;
	}
}


//--- message handlers
void xswitch_up(struct xswitch *sw)
{
	init_counter_table(sw);
	init_table0(sw);
	sw->next_table_id = 2;
	core_switch_up(sw);
	topo_switch_up(sw);
}

void xswitch_packet_in(struct xswitch *sw, int in_port, uint8_t *packet, int packet_len)
{
	int i;
	xdebug("packet in, dpid: 0x%x, in_port: %u, total_len: %u\n",
	       sw->dpid, in_port, packet_len);
	for(i = 0; i < packet_len; i++)
	{
		if(i%16 == 0)
			xdebug("%06x ", i);
		xdebug("%02x%c", packet[i], i%16==15 ? '\n' : ' ');
	}
	xdebug("\n");
	if (!topo_packet_in(sw, in_port, packet, packet_len))
		core_packet_in(sw, in_port, packet, packet_len);
}

void xswitch_port_status(struct xswitch *sw, int port, enum port_status status)
{
	topo_switch_port_status(sw, port, status);
}

void xswitch_down(struct xswitch *sw)
{
	int i;
	struct xport **xps;
	topo_switch_down(sw);
	core_switch_down(sw);
	flow_table_free(sw->table0);
	xps = xswitch_get_xports(sw);
	for (i = 0; i < XPORT_HASH_SIZE; i++) {
		if (xps[i] != NULL)
			free(xps[i]);
	}
}

void xswitch_on_timeout(void)
{
	int i;
	struct xport **xps, *xp;
	struct xswitch *sw;
	uint16_t port_id;
	struct msgbuf *msg;
	int j;
	for (i = 0; i < xswitchnum; i++) {
		sw = xswitches[i];
		xps = xswitch_get_xports(sw);
		for (j = 0; j < XPORT_HASH_SIZE; j++) {
			xp = xps[j];
			if (xp == NULL)
				continue;
next2:
			port_id = xport_get_port_id(xp);
			msg = msg_counter_request(port_id);
			xswitch_send(sw, msg);
			if ((xp = xport_get_next(xp)) != NULL)
				goto next2;
		}
	}
#if 0
	// static int sendonce = 0;
	struct msgbuf *msg;
	struct xswitch *sw;
	int i;
	int counter_id;
	/*if (sendonce)
		return;
	sendonce = 1;
	*/
	xdebug("\n\n\nsend counter request\n");
	counter_id = 1;
	for (i = 0; i < xswitchnum; i++) {
		sw = xswitches[i];
		msg = msg_counter_request(counter_id);
		xswitch_send(sw, msg);
	}
#endif
}
