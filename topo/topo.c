#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "xswitch/xswitch.h"
#include "topo.h"
#include "entity-private.h"
#include "discovery.h"

static struct entity *hosts[100];
static int num_hosts;
static struct entity *switches[100];
static int num_switches;

void topo_print()
{
	int i;
	fprintf(stderr, "\nSwitches--------------\n");
	for (i=0; i<num_switches; i++) {
		entity_print(switches[i]);
	}
	fprintf(stderr, "\nHosts--------------\n");
	for (i=0; i<num_hosts; i++) {
		entity_print(hosts[i]);
	}
	fprintf(stderr, "num_switches: %d, num_hosts: %d\n",
		num_switches, num_hosts);
}

int topo_add_host(struct entity *e)
{
	int i;
	for (i=0; i < num_hosts; i++)
		if (hosts[i] == e)
			return i;
	if (num_hosts >= 100)
		return -1;
	hosts[num_hosts++] = e;
	return (num_hosts-1);
}

int topo_add_switch(struct entity *e)
{
	int i;
	for (i=0; i<num_switches; i++)
		if (switches[i] == e)
			return i;
	if(num_switches >= 100)
		return -1;
	switches[num_switches++] = e;
	return (num_switches - 1);
}

int topo_del_host(struct entity *e)
{
	int i;
	for (i=0; i < num_hosts; i++)
		if (hosts[i] == e) {
			num_hosts --;
			entity_free(e);
			if (i != num_hosts)
				hosts[i] = hosts[num_hosts];
			return i;
		}
	return -1;
}

int topo_del_switch(struct entity *e)
{
	int i;
	for (i=0; i < num_switches; i++)
		if (switches[i] == e) {
			num_switches --;
			entity_free(e);
			if (i != num_switches)
				switches[i] = switches[num_switches];
			return i;
		}
	return -1;
}

struct entity *topo_get_host_by_haddr(haddr_t addr)
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

struct entity *topo_get_switch(dpid_t dpid)
{
	int i;
	for(i = 0; i < num_switches; i++) {
		struct xswitch *xs = entity_get_xswitch(switches[i]);
		if(xswitch_get_dpid(xs) == dpid) {
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
	topo_add_switch(e);
	lldp_packet_send(sw);
}

bool topo_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len)
{
	int rt = handle_topo_packet_in(sw, in_port, packet, packet_len);
	fprintf(stderr, "return value %d\n", rt);
	if (rt==-2)
		return false;
	return true;
}

void topo_switch_port_down(struct xswitch *sw, int port)
{
	int i;
	int sw_port;
	struct entity *esw;
	struct xswitch *xsw;
	for (i = 0; i < num_hosts; i++) {
		esw = entity_host_get_adj_switch(hosts[i], &sw_port);
		xsw = entity_get_xswitch(esw);
		if (xsw == sw && sw_port == port) {
			topo_del_host(hosts[i]);
		}
	}
}

void topo_switch_down(struct xswitch *sw)
{
	int j;
	struct entity *esw = topo_get_switch(xswitch_get_dpid(sw));

	fprintf(stderr, "-----switch down--------\n");
	topo_del_switch(esw);
	for (j = 0; j < num_hosts;) {
		int num_adjs;
		entity_get_adjs(hosts[j], &num_adjs);
		if(num_adjs == 0) {
			num_hosts--;
			entity_free(hosts[j]);
			hosts[j] = hosts[num_hosts];
		} else {
			j++;
		}
	}
	topo_print();
}
