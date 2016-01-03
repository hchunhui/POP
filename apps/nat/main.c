#include <assert.h>
#include <stdio.h>
#include "pop_api.h"
#include "route.h"
#include "map.h"

/* util */
static struct route *forward(struct entity *esw, int in_port, int out_port)
{
	struct route *r = route();
	route_add_edge(r, edge(NULL, 0, esw, in_port));
	route_add_edge(r, edge(esw, out_port, NULL, 0));
	return r;
}

/* data structures */
struct ip_port {
	uint32_t ip;
	uint16_t port;
};

struct ip_port *ip_port(uint32_t ip, uint16_t port)
{
	struct ip_port *i;
	i = malloc(sizeof(struct ip_port));
	i->ip = ip;
	i->port = port;
	return i;
}

static bool ipp_eq(int_or_ptr_t p1, int_or_ptr_t p2)
{
	struct ip_port *i1 = p1.p;
	struct ip_port *i2 = p2.p;
	return (i1->ip == i2->ip &&
		i1->port == i2->port);
}

static void ipp_free(int_or_ptr_t p)
{
	free(p.p);
}

struct conn_info {
	uint32_t sip, dip;
	uint16_t sport, dport;
};

struct conn_info *conn_info(uint32_t sip, uint32_t dip,
			   uint16_t sport, uint16_t dport)
{
	struct conn_info *c;
	c = malloc(sizeof(struct conn_info));
	c->sip = sip;
	c->dip = dip;
	c->sport = sport;
	c->dport = dport;
	return c;
}

static bool ci_eq(int_or_ptr_t p1, int_or_ptr_t p2)
{
	struct conn_info *c1 = p1.p;
	struct conn_info *c2 = p2.p;
	return (c1->sip == c2->sip &&
		c1->dip == c2->dip &&
		c1->sport == c2->sport &&
		c1->dport == c2->dport);
}

static unsigned int ci_hash(int_or_ptr_t p)
{
	struct conn_info *c = p.p;
	return c->sip + c->dip + c->sport + c->dport;
}

static int_or_ptr_t ci_dup(int_or_ptr_t p)
{
	struct conn_info *c0 = p.p;
	return PTR(conn_info(c0->sip, c0->dip, c0->sport, c0->dport));
}

static void ci_free(int_or_ptr_t p)
{
	free(p.p);
}

/*
 * NAT (UDP only)
 * (1) network configuration:
 *     PRIVATE_NET <--> (port 1) SWITCH (port 2) <--> PUBLIC_NET
 * (2) tables
 *     snat_table : (sip, sport, dip, dport) -> new_sport
 *     dnat_table : (sip, sport, dip, dport) -> (orig_dip, orig_dport)
 */

#define MY_PRIVATE_MAC 0xffffffffffffull
#define MY_PUBLIC_MAC 0xffffffffffffull
#define MY_PUBLIC_IP 0x0b0b0b0b

enum action {
	PASS,
	DROP,
};

static enum action snat(struct packet *pkt, struct map *snat_table, struct map *dnat_table)
{
	static uint16_t next_port = 2000;

	pull_header(pkt);
	if(strcmp(read_header_type(pkt), "ipv4") == 0) {
		uint32_t sip = value_to_32(read_packet(pkt, "nw_src"));
		uint32_t dip = value_to_32(read_packet(pkt, "nw_dst"));

		pull_header(pkt);
		if(strcmp(read_header_type(pkt), "udp") == 0) {
			uint16_t sport = value_to_16(read_packet(pkt, "tp_src"));
			uint16_t dport = value_to_16(read_packet(pkt, "tp_dst"));
			struct conn_info *ci = conn_info(sip, dip, sport, dport);
			uint16_t new_sport = map_read(snat_table, PTR(ci)).v;
			if(new_sport == 0) {
				new_sport = next_port;
				next_port++;

				struct conn_info *dci = conn_info(dip, MY_PUBLIC_IP, dport, new_sport);
				map_add_key(snat_table, PTR(ci), INT(new_sport), mapf_eq_int, mapf_free_int);
				map_add_key(dnat_table, PTR(dci), PTR(ip_port(sip, sport)), ipp_eq, ipp_free);
				free(dci);
			}
			free(ci);
			mod_packet(pkt, "tp_src", value_from_16(new_sport));
			mod_packet(pkt, "sum", value_from_16(0));

			push_header(pkt);
			mod_packet(pkt, "nw_src", value_from_32(MY_PUBLIC_IP));

			push_header(pkt);
			mod_packet(pkt, "dl_src", value_from_48(MY_PUBLIC_MAC));
			mod_packet(pkt, "dl_dst", value_from_48(0xffffffffffffull));
			return PASS;
		}
	}
	return DROP;
}

static enum action dnat(struct packet *pkt, struct map *dnat_table)
{
	pull_header(pkt);
	if(strcmp(read_header_type(pkt), "ipv4") == 0) {
		uint32_t sip = value_to_32(read_packet(pkt, "nw_src"));
		uint32_t dip = value_to_32(read_packet(pkt, "nw_dst"));

		pull_header(pkt);
		if(strcmp(read_header_type(pkt), "udp") == 0) {
			uint16_t sport = value_to_16(read_packet(pkt, "tp_src"));
			uint16_t dport = value_to_16(read_packet(pkt, "tp_dst"));
			struct conn_info *ci = conn_info(sip, dip, sport, dport);
			struct ip_port *ipp = map_read(dnat_table, PTR(ci)).p;
			free(ci);
			if(ipp) {
				mod_packet(pkt, "tp_dst", value_from_16(ipp->port));
				mod_packet(pkt, "sum", value_from_16(0));

				push_header(pkt);
				mod_packet(pkt, "nw_dst", value_from_32(ipp->ip));

				push_header(pkt);
				mod_packet(pkt, "dl_src", value_from_48(MY_PRIVATE_MAC));
				mod_packet(pkt, "dl_dst", value_from_48(0xffffffffffffull));
				return PASS;
			}
		}
	}
	return DROP;
}


/* f */
void init_f(struct map *env)
{
	map_add_key(env, PTR("snat_table"),
		    PTR(map(ci_eq, ci_hash, ci_dup, ci_free)),
		    mapf_eq_map, mapf_free_map);
	map_add_key(env, PTR("dnat_table"),
		    PTR(map(ci_eq, ci_hash, ci_dup, ci_free)),
		    mapf_eq_map, mapf_free_map);
}

struct route *f(struct packet *pkt, struct map *env)
{
	struct entity *me = read_packet_inswitch(pkt);
	dpid_t dpid = get_switch_dpid(me);
	int in_port = read_packet_inport(pkt);
	struct map *snat_table = map_read(env, PTR("snat_table")).p;
	struct map *dnat_table = map_read(env, PTR("dnat_table")).p;
	assert(snat_table && dnat_table);

	if(dpid == 1) {
		if(in_port == 1) {
			if(snat(pkt, snat_table, dnat_table) == PASS)
				return forward(me, in_port, 2);
			else
				return route();
		} else if(in_port == 2) {
			if(dnat(pkt, dnat_table) == PASS)
				return forward(me, in_port, 1);
			else
				return route();
		}
	}
	return route();
}
