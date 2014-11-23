#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "xswitch/xswitch-private.h"
#include "topo.h"
#include "entity-private.h"
#include "discovery.h"
#include "packet_in.h"
// TODO next for delete
// TODO delete
static struct entity *hosts[100];
static int num_hosts;
static struct entity *switches[100];
static int num_switches;
/*
static int next_num(struct entity **e)
{
	int i;
	for (i = 0; i < 100; i++)
		if (e[i] == NULL)
			return i;
	return i;
}
*/
void topo_print()
{
	int i;
	printf("\n\nSwitches--------------\n");
	for (i=0; i<num_switches; i++) {
		entity_print(switches[i]);
	}
	printf("\n\nHosts--------------\n");
	for (i=0; i<num_hosts; i++) {
		entity_print(hosts[i]);
	}
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
	switches[num_switches++] = e;
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
	int rt = handle_topo_packet_in(&pkt_in);
	printf("return value %d\n", rt);
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
			entity_adj_down(esw, port);
			entity_free(hosts[i]);
			num_hosts --;
			if (i != num_hosts)
				hosts[i] = hosts[num_hosts];
		}
	}
}
void topo_switch_down(struct xswitch *sw)
{
	printf("-----switch down--------\n");
	int i, j;
	int num_adjs;
	const struct entity_adj *e_adjs;
	int sw_port;
	for(i = 0; i < num_switches; i++) {
		if(entity_get_xswitch(switches[i]) == sw) {
			e_adjs = entity_get_adjs(switches[i], &num_adjs);
			for (j = 0; j < num_adjs; j++) {
				if(ENTITY_TYPE_SWITCH == entity_get_type(e_adjs[j].adj_entity)) {
					entity_adj_down(e_adjs[j].adj_entity, e_adjs[j].adj_in_port);
				}
			}
			for (j = 0; j < num_hosts; j++) {
				// printf("e: %p  sw_port=%p\n", hosts[j], &sw_port);
				if(entity_host_get_adj_switch(hosts[j], &sw_port) == switches[i]) {
				// int ret = entity_host_get_adj_switch(hosts[j], &sw_port);
				// printf("ret = %016x\n", ret);
				// if(ret == switches[i]) {
					entity_free(hosts[j]);
					num_hosts --;
					if (j != num_hosts)
						hosts[j] = hosts[num_hosts];
				}
			}
			// free hosts
			entity_free(switches[i]);
			num_switches --;
			if (i != num_switches)
				switches[i] = switches[num_switches];
			/* hack */
			// for(j = 0; j < num_hosts; j++)
			//	entity_free(hosts[j]);
			// num_hosts = 0;
			break;
		}
	// abort();
	}
	topo_print();
}
