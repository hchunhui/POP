#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "xswitch/xswitch.h"
#include "topo.h"
#include "entity.h"
#include "discovery.h"

#include "maple/maple.h"

#define MAX_NUM_HOSTS 1000
#define MAX_NUM_SWITCHES 100
static struct entity *hosts[MAX_NUM_HOSTS];
static int num_hosts;
static struct entity *switches[MAX_NUM_SWITCHES];
static int num_switches;

void topo_print(void)
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

static bool host_p(void *phost, const char *name, void *arg)
{
	if(strcmp(name, "topo_hosts") == 0 &&
	   hosts == arg)
		return true;
	if(strcmp(name, "topo_host") == 0 &&
	   arg == phost)
		return true;
#ifdef STRICT_INVALIDATE
	if(phost && strncmp(name, "entity_adjs", 11) == 0) {
		int i;
		int n = atoi(name + 11);
		const struct entity_adj *adjs = arg;
		for(i = 0; i < n; i++)
			if(adjs[i].adj_entity == phost)
				return true;
	}
#endif
	return false;
}

static bool switch_p(void *pswitch, const char *name, void *arg)
{
	if(strcmp(name, "topo_switches") == 0 &&
	   switches == arg)
		return true;
	if(strcmp(name, "topo_switch") == 0 &&
	   arg == pswitch)
		return true;
	if(pswitch && strncmp(name, "entity_adjs", 11) == 0) {
		int i;
		int n = atoi(name + 11);
		const struct entity_adj *adjs = arg;
		for(i = 0; i < n; i++)
			if(adjs[i].adj_entity == pswitch)
				return true;
	}
	return false;
}

int topo_add_host(struct entity *e)
{
	int i;
	for (i=0; i < num_hosts; i++)
		if (hosts[i] == e)
			return i;
	if (num_hosts >= MAX_NUM_HOSTS)
		return -1;
	hosts[num_hosts++] = e;
	maple_invalidate(host_p, NULL);
	return (num_hosts-1);
}


int topo_add_switch(struct entity *e)
{
	int i;
	for (i=0; i<num_switches; i++)
		if (switches[i] == e)
			return i;
	if(num_switches >= MAX_NUM_SWITCHES)
		return -1;
	switches[num_switches++] = e;
	maple_invalidate(switch_p, NULL);
	return (num_switches - 1);
}

int topo_del_host(struct entity *e)
{
	int i;
	for (i=0; i < num_hosts; i++)
		if (hosts[i] == e) {
			maple_invalidate(host_p, e);
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
			maple_invalidate(switch_p, e);
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

static void del_dangling_hosts(void)
{
	int j;
	for (j = 0; j < num_hosts;) {
		int num_adjs;
		entity_get_adjs(hosts[j], &num_adjs);
		if(num_adjs == 0) {
			maple_invalidate(host_p, hosts[j]);
			num_hosts--;
			entity_free(hosts[j]);
			hosts[j] = hosts[num_hosts];
		} else {
			j++;
		}
	}
}

void topo_switch_port_status(struct xswitch *sw, int port, enum port_status status)
{
	struct entity *esw = topo_get_switch(xswitch_get_dpid(sw));
	switch(status) {
	case PORT_DOWN:
		fprintf(stderr, "-----port down-----\n");
		entity_del_link(esw, port);
		del_dangling_hosts();
		topo_print();
		break;
	case PORT_UP:
		fprintf(stderr, "-----port up-----\n");
		lldp_packet_send(sw);
		break;
	}
}

void topo_switch_down(struct xswitch *sw)
{
	struct entity *esw = topo_get_switch(xswitch_get_dpid(sw));

	fprintf(stderr, "-----switch down--------\n");
	topo_del_switch(esw);
	del_dangling_hosts();
	topo_print();
}
