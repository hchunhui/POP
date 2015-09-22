#include <stdio.h>
#include "pop_api.h"
#include "route.h"

void init_f(struct map *env) {}

static struct route *forward(struct entity *esw, int in_port, int out_port)
{
	struct route *r = route();
	route_add_edge(r, edge(NULL, 0, esw, in_port));
	route_add_edge(r, edge(esw, out_port, NULL, 0));
	return r;
}

static struct route *forward2(struct entity *esw, int in_port, int out_port1, int out_port2)
{
	struct route *r = route();
	route_add_edge(r, edge(NULL, 0, esw, in_port));
	route_add_edge(r, edge(esw, out_port1, NULL, 0));
	route_add_edge(r, edge(esw, out_port2, NULL, 0));
	return r;
}

static void compress(struct packet *pkt)
{
	uint16_t dl_type = value_to_16(read_packet(pkt, "dl_type"));

	if(dl_type == 0x0800) {
		uint8_t nw_proto = value_to_8(read_packet(pkt, "nw_proto"));
		if(nw_proto == 0x11) {
			mod_packet(pkt, "dl_type", value_from_16(0x0901));
			del_field(pkt, 8*(14+20), 8*4);
			del_field(pkt, 8*(14+12), 8*8);
			del_field(pkt, 8*(14+8), 8*2);
			del_field(pkt, 8*14, 8*2);
		}
	}
}

static void decompress(struct packet *pkt, uint64_t ips)
{
	uint16_t dl_type = value_to_16(read_packet(pkt, "dl_type"));

	if(dl_type == 0x0901) {
		add_field(pkt, 8*14, 8*2, value_from_16(0x4500));
		add_field(pkt, 8*(14+8), 8*2, value_from_16(0x4011));
		add_field(pkt, 8*(14+12), 8*8, value_from_64(ips));
		add_field(pkt, 8*(14+20), 8*4, value_from_32((1234<<16) | 1234));
		mod_packet(pkt, "dl_type", value_from_16(0x0800));
	}
}

struct route *f(struct packet *pkt, struct map *env)
{
	struct entity *me = read_packet_inswitch(pkt);
	dpid_t dpid = get_switch_dpid(me);
	int in_port = read_packet_inport(pkt);
	if(dpid == 1) {
		if(in_port == 1) {
			return forward(me, in_port, 2);
		} else if(in_port == 2) {
			decompress(pkt, 0x0a0000030a000001ull);
			return forward(me, in_port, 1);
		}
	} else if(dpid == 2) {
		if(in_port == 1)
			return forward2(me, in_port, 2, 3);
		else if(in_port == 2)
			return forward2(me, in_port, 1, 3);
	} else if(dpid == 3) {
		if(in_port == 1) {
			return forward(me, in_port, 2);
		} else if (in_port == 2) {
			compress(pkt);
			return forward(me, in_port, 1);
		}
	}
	return route();
}
