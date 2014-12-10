#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "xswitch/xswitch-private.h"
#include "topo/topo.h"
#include "topo/entity.h"
#include "route.h"

#include "maple.h"
#include "packet_parser.h"
#include "spec_parser.h"
#include "trace.h"
#include "trace_tree.h"

#include "maple_api.h"

static struct header *header_spec;

/* f */
struct route *f(struct packet *pk);

/* API for f */
struct packet {
	struct packet_parser *pp;
	bool hack_get_payload;
};

void pull_header(struct packet *pkt)
{
	value_t sel_value;
	struct header *old_spec, *new_spec;
	/* XXX: hack */
	packet_parser_pull(pkt->pp, &old_spec, &sel_value, &new_spec);
	trace_R(header_get_sel(old_spec), sel_value);
	trace_G(new_spec, header_get_length(old_spec));
}

const char *read_header_type(struct packet *pkt)
{
	return packet_parser_read_type(pkt->pp);
}

value_t read_packet(struct packet *pkt, const char *field)
{
	value_t v = packet_parser_read(pkt->pp, field);
	trace_R(field, v);
	return v;
}

bool test_equal(struct packet *pkt, const char *field, value_t value)
{
	value_t v = packet_parser_read(pkt->pp, field);
	bool result = value_equal(v, value);
	trace_T(field, value, result);
	return result;
}

const uint8_t *read_payload(struct packet *pkt, int *length)
{
	pkt->hack_get_payload = true;
	return packet_parser_get_payload(pkt->pp, length);
}

void record(const char *name)
{
	trace_RE(name, NULL);
}

void invalidate(const char *name)
{
	trace_IE(name);
}

struct entity **get_hosts(int *pnum)
{
	struct entity **hosts = topo_get_hosts(pnum);
	trace_RE("topo_hosts", (void *)hosts);
	return hosts;
}

struct entity **get_switches(int *pnum)
{
	struct entity **switches = topo_get_switches(pnum);
	trace_RE("topo_switches", (void *)switches);
	return switches;
}

struct entity *get_switch(dpid_t dpid)
{
	struct entity *esw = topo_get_switch(dpid);
	trace_RE("topo_switch", (void *)esw);
	return esw;
}

struct entity *get_host_by_haddr(haddr_t addr)
{
	struct entity *eh = topo_get_host_by_haddr(addr);
	trace_RE("topo_host", (void *)eh);
	return eh;
}

struct entity *get_host_by_paddr(uint32_t addr)
{
	struct entity *eh = topo_get_host_by_paddr(addr);
	trace_RE("topo_host", (void *)eh);
	return eh;
}

enum entity_type get_entity_type(struct entity *e)
{
	return entity_get_type(e);
}

dpid_t get_switch_dpid(struct entity *e)
{
	return entity_get_dpid(e);
}

const struct entity_adj *get_entity_adjs(struct entity *e, int *pnum)
{
	const struct entity_adj *adjs = entity_get_adjs(e, pnum);
	char buf[32];
	snprintf(buf, 32, "entity_adjs%d", *pnum);
	trace_RE(buf, (void *)adjs);
	return adjs;
}

struct entity *get_host_adj_switch(struct entity *e, int *sw_port)
{
	assert(entity_get_type(e) == ENTITY_TYPE_HOST);
	int num;
	const struct entity_adj *adjs = get_entity_adjs(e, &num);
	assert(num == 1);
	*sw_port = adjs[0].adj_in_port;
	return adjs[0].adj_entity;
}

void print_entity(struct entity *e)
{
	return entity_print(e);
}

/* XXX */
void maple_invalidate(bool (*p)(void *p_data, const char *name, void *arg), void *p_data)
{
	int num_switches;
	struct entity **switches = topo_get_switches(&num_switches);
	int i;
	fprintf(stderr, "maple_invalidate\n");
	for(i = 0; i < num_switches; i++) {
		struct xswitch *cur_sw = entity_get_xswitch(switches[i]);
		struct trace_tree *tt = cur_sw->trace_tree;
		if(trace_tree_invalidate(&tt, p, p_data)) {
			fprintf(stderr, "---- flow table for 0x%x ---\n", cur_sw->dpid);
			trace_tree_emit_rule(cur_sw, cur_sw->trace_tree);
		}
	}
}

/* call back funtions */
void maple_init(void)
{
	fprintf(stderr, "loading header spec...\n");
	header_spec = spec_parser_file("scripts/header.spec");
	assert(header_spec);
}

void maple_switch_up(struct xswitch *sw)
{
	/* init trace tree */
	sw->trace_tree = trace_tree();
}

void maple_switch_down(struct xswitch *sw)
{
	trace_tree_free(sw->trace_tree);
}

static void mod_in_port(struct trace *trace, int in_port)
{
	int i = trace->num_events - 1;
	assert(i >= 0);
	assert(trace->events[i].type == EV_R);
	assert(strcmp(trace->events[i].u.r.name, "in_port") == 0);
	trace->events[i].u.r.value = value_from_8(in_port);
}

static bool cmpname_p(void *pname, const char *name, void *arg)
{
	if(strcmp(pname, name) == 0)
		return true;
	return false;
}

void maple_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len)
{
	int i;
	struct route *r;
	struct packet pkt;
	edge_t *edges;
	int num_edges;
	struct trace *trace;

	/* init */
	trace_clear();

	/* run */
	pkt.pp = packet_parser(header_spec, packet, packet_len);
	pkt.hack_get_payload = false;
	trace_G(header_spec, expr_value(0));

	r = f(&pkt);

	trace_R("in_port", value_from_8(0));
	packet_parser_free(pkt.pp);

	trace = trace_get();

	/* learn */
	edges = route_get_edges(r, &num_edges);
	for(i = 0; i < num_edges; i++) {
		struct entity *cur_ent = edges[i].ent1;
		int out_port = edges[i].port1;
		fprintf(stderr, "handle edge (0x%x, %d, 0x%x, %d):\n",
			entity_get_dpid(edges[i].ent1),
			edges[i].port1,
			entity_get_dpid(edges[i].ent2),
			edges[i].port2);
		if(cur_ent) {
			struct xswitch *cur_sw = entity_get_xswitch(cur_ent);
			struct action *a = action();
			int j;

			action_add(a, AC_OUTPUT, out_port);

			assert(cur_sw);

			if(!edges[i].ent2) {
				struct msgbuf *mb;
				mb = msg_packet_out(0, packet, packet_len, a);
				xswitch_send(cur_sw, mb);
			}

			for(j = 0; j < num_edges; j++) {
				if(edges[j].ent2 == cur_ent)
					break;
			}
			assert(j < num_edges);
			mod_in_port(trace, edges[j].port2);

			if(trace_tree_augment(&(cur_sw->trace_tree), trace, a)) {
				fprintf(stderr, "--- flow table for 0x%x ---\n", entity_get_dpid(cur_ent));
				trace_tree_print(cur_sw->trace_tree);
				fprintf(stderr, "\n");
				trace_tree_emit_rule(cur_sw, cur_sw->trace_tree);
			}
			action_free(a);
		}
	}

	if((!pkt.hack_get_payload) && num_edges == 0) {
		struct action *a = action();
		mod_in_port(trace, in_port);
		action_add(a, AC_DROP, 0);
		if(trace_tree_augment(&(sw->trace_tree), trace, a)) {
			fprintf(stderr, "--- flow table for cur sw ---\n");
			trace_tree_print(sw->trace_tree);
			fprintf(stderr, "\n");
			trace_tree_emit_rule(sw, sw->trace_tree);
		}
		action_free(a);
	}

	route_free(r);

	/* invalidate */
	for(i = 0; i < trace->num_inv_events; i++) {
		const char *name;
		name = trace->inv_events[i].name;
		fprintf(stderr, "invalidate \"%s\":\n", name);
		maple_invalidate(cmpname_p, (void *)name);
	}
}
