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

static int next_num(struct entity **e)
{
	int i;
	for (i = 0; i < 100; i++)
		if (e[i] == NULL)
			return i;
	return i;
}

bool topo_add_host(struct entity *e)
{
	int i;
	for (i=0; i<100; i++)
		if (hosts[i] == e)
			return true;
	if(num_hosts >= 100)
		return false;
	hosts[num_hosts] = e;
	num_hosts = next_num(hosts);
	return true;
}
bool topo_add_switch(struct entity *e)
{
	int i;
	for (i=0; i<100; i++)
		if (switches[i] == e)
			return true;
	if(num_switches >= 100)
		return false;
	switches[num_switches] = e;
	num_switches = next_num(switches);
	return true;
}
bool topo_del_host(struct entity *e)
{
	int i;
	for (i=0; i<100; i++)
		if (hosts[i] == e) {
			hosts[i] = NULL;
			entity_free(e);
			break;
		}
	if (i < num_hosts)
		num_hosts = i;
	if (i > 100)
		return false;
	return true;
}
bool topo_del_switch(struct entity *e)
{
	int i;
	for (i=0; i<100; i++)
		if (switches[i] == e) {
			switches[i] = NULL;
			entity_free(e);
			break;
		}
	if (i < num_switches)
		num_switches = i;
	if (i > 100)
		return false;
	return true;
}
struct entity *topo_get_host_by_haddr(struct haddr addr)
{
	int i;
	struct host_info h1;
	for (i = 0; i < num_hosts; i++) {
		h1 = entity_get_addr(hosts[i]);
		if (haddr_equal(h1.haddr, addr))
			return hosts[i];
	}
	return NULL;
}
struct entity *topo_get_host_by_paddr(uint32_t addr)
{
	int i;
	struct host_info h1;
	for (i = 0; i < num_hosts; i++) {
		h1 = entity_get_addr(hosts[i]);
		if (h1.paddr == addr)
			return hosts[i];
	}
	return NULL;
}

struct entity *topo_get_host(value_t addr)
{
	int i;
	struct host_info hinfo;
	for(i = 0; i < num_hosts; i++) {
		hinfo = entity_get_addr(hosts[i]);
		if(haddr_equal(hinfo.haddr, value_to_haddr(addr)))
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
	num_switches = next_num(switches);
	lldp_flow_install(sw, 2);
	lldp_packet_send(sw);
	arp_default_flow_install(sw, 1);
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
	int rt = handle_topo_packet_in(&pkt_in);
	printf("return value %d\n", rt);
	if (rt==-2)
		return false;
	return true;
}

void topo_switch_down(struct xswitch *sw)
{
	int i, j;
	for(i = 0; i < num_switches; i++) {
		if(entity_get_xswitch(switches[i]) == sw) {
			entity_free(switches[i]);
			num_switches = next_num(switches);
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
