#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "xswitch-private.h"
#include "topo.h"
#include "entity.h"
#include "discovery.h"
#include "packet_in.h"

static struct entity *hosts[100];
static int num_hosts;
static struct entity *switches[100];
static int num_switches;

struct entity *topo_get_host(value_t addr)
{
	int i;
	for(i = 0; i < num_hosts; i++) {
		if(value_equ(entity_get_addr(hosts[i]), addr))
			return hosts[i];
	}
	return NULL;
}

struct entity *topo_get_switch(dpid_t dpid)
{
	int i;
	for(i = 0; i < num_switches; i++) {
		struct xswitch *xs = entity_get_xswitch(switches[i]);
		if(xs->dpid == dpid) {
			return switches[i];
		}
	}
	return NULL;
}

struct entity **topo_get_hosts(int *pnum)
{
	*pnum = num_hosts;
	return hosts;
}

struct entity **topo_get_switches(int *pnum)
{
	*pnum = num_switches;
	return switches;
}

void topo_switch_up(struct xswitch *sw)
{
	struct entity *e = entity_switch(sw);
	if(num_switches >= 100)
		abort();
	switches[num_switches] = e;
	num_switches++;
	lldp_flow_install(sw, 1);
	lldp_packet_send(sw);
	/* hack */
	/*
	if(num_switches == 4) {
		fprintf(stderr, "!!!hack hack hack!!!\n");
		struct entity *s1 = topo_get_switch(1);
		struct entity *s2 = topo_get_switch(2);
		struct entity *s3 = topo_get_switch(3);
		struct entity *s4 = topo_get_switch(4);
		struct entity *h1 = entity_host(value_from_48(1));
		struct entity *h2 = entity_host(value_from_48(2));
		struct entity *h3 = entity_host(value_from_48(3));
		struct entity *h4 = entity_host(value_from_48(4));
		hosts[num_hosts] = h1;
		num_hosts++;
		hosts[num_hosts] = h2;
		num_hosts++;
		hosts[num_hosts] = h3;
		num_hosts++;
		hosts[num_hosts] = h4;
		num_hosts++;
		entity_add_link(h1, 1, s1, 1);
		entity_add_link(h2, 1, s2, 1);
		entity_add_link(h3, 1, s3, 1);
		entity_add_link(h4, 1, s4, 1);

		entity_add_link(s1, 2, s2, 2);
		entity_add_link(s2, 3, s3, 3);
		entity_add_link(s3, 2, s4, 2);
		entity_add_link(s4, 3, s1, 3);
		entity_add_link(s1, 4, s3, 4);
	}
	*/
}
/*
struct packet_in
{
	uint8_t *packet;
	int packet_len;
	dpid_t dpid;
	port_t port;
}*/
bool topo_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len)
{
	struct packet_in pkt_in = {
		packet,
		packet_len,
		in_port,
		sw->dpid,
	};
	if (handle_lldp_packet_in(&pkt_in)==-2)
		return false;
	return true;
}

void topo_switch_down(struct xswitch *sw)
{
	int i, j;
	for(i = 0; i < num_switches; i++) {
		if(entity_get_xswitch(switches[i]) == sw) {
			entity_free(switches[i]);
			num_switches--;
			switches[i] = switches[num_switches];
			/* hack */
			for(j = 0; j < num_hosts; j++)
				entity_free(hosts[j]);
			num_hosts = 0;
			return;
		}
	}
	abort();
}
