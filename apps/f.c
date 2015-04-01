#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "maple_api.h"
#include "route.h"

#include "igmp.h"
#include "spanning_tree.h"
#include "map.h"

static struct route *handle_sdnp(struct packet *pkt, struct map *env)
{
	int switches_num;
	struct entity **switches = get_switches(&switches_num);
	struct route *r;
	struct entity *src, *dst;
	int src_port, dst_port;
	struct nodeinfo *visited;

	src = get_switch(value_to_32(read_packet(pkt, "dpid_src")));
	dst = get_switch(value_to_32(read_packet(pkt, "dpid_dst")));
	src_port = value_to_16(read_packet(pkt, "port_src"));
	dst_port = value_to_16(read_packet(pkt, "port_dst"));

	if(src == NULL || dst == NULL) {
		fprintf(stderr, "sdnp: bad address.\n");
		return route();
	}

	visited = get_tree(src, src_port, dst, switches, switches_num);
	r = get_route(dst, dst_port, visited, switches, switches_num);
	free(visited);
	return r;
}

static struct route *handle_ipv4_unicast(uint32_t hsrc_ip, uint32_t hdst_ip, struct map *env)
{
	int switches_num;
	struct entity **switches = get_switches(&switches_num);
	struct route *r;
	struct entity *hsrc, *hdst;
	struct entity *src, *dst;
	int src_port, dst_port;
	struct nodeinfo *visited;

	hsrc = get_host_by_paddr(hsrc_ip);
	hdst = get_host_by_paddr(hdst_ip);
	if(hsrc == NULL || hdst == NULL) {
		fprintf(stderr, "ipv4_unicast: bad address.\n");
		return route();
	}
	src = get_host_adj_switch(hsrc, &src_port);
	dst = get_host_adj_switch(hdst, &dst_port);

	visited = get_tree(src, src_port, dst, switches, switches_num);
	r = get_route(dst, dst_port, visited, switches, switches_num);
	free(visited);
	return r;
}

static struct route *handle_ipv4_multicast(uint32_t hsrc_ip, uint32_t hdst_ip, struct map *env)
{
	int i;
	int switches_num;
	struct entity **switches = get_switches(&switches_num);
	struct route *rx, *r;
	struct entity *hsrc, *hdst;
	struct entity *src, *dst;
	int src_port, dst_port;
	struct nodeinfo *visited;

	struct igmp_addrs *l = igmp_get_maddrs(map_read(env, PTR("group_table")).p, hdst_ip);
	r = route();

	hsrc = get_host_by_paddr(hsrc_ip);
	if(hsrc == NULL) {
		fprintf(stderr, "ipv4_multicast: bad src address.\n");
		return route();
	}
	src = get_host_adj_switch(hsrc, &src_port);

	visited = get_tree(src, src_port, NULL, switches, switches_num);
	fprintf(stderr, "group_id: %08x, group_n: %d\n", hdst_ip, l->n);
	for(i = 0; i < l->n; i++) {
		hdst = get_host_by_paddr(l->addrs[i]);
		if(hdst == NULL) {
			fprintf(stderr, "ipv4_multicast: bad dst address.\n");
			continue;
		}

		dst = get_host_adj_switch(hdst, &dst_port);
		rx = get_route(dst, dst_port, visited, switches, switches_num);
		route_union(r, rx);
		route_free(rx);
	}
	free(visited);
	return r;
}

static struct route *handle_igmp(struct packet *pkt, struct map *env)
{
	int len;
	uint32_t hsrc_ip = value_to_32(read_packet(pkt, "nw_src"));
	const uint8_t *payload = read_payload(pkt, &len);
	process_igmp(map_read(env, PTR("group_table")).p, hsrc_ip, payload, len);
	return route();
}

static bool is_multicast_ip(uint32_t ip)
{
	if((ip >> 24) >= 224 && (ip >> 24) < 240)
		return true;
	return false;
}

static struct route *handle_ipv4(struct packet *pkt, struct map *env)
{
	if(test_equal(pkt, "nw_proto", value_from_8(0x02))) {
		return handle_igmp(pkt, env);
	} else {
		uint32_t hsrc_ip = value_to_32(read_packet(pkt, "nw_src"));
		uint32_t hdst_ip = value_to_32(read_packet(pkt, "nw_dst"));
		if(is_multicast_ip(hdst_ip)) {
			return handle_ipv4_multicast(hsrc_ip, hdst_ip, env);
		} else {
			return handle_ipv4_unicast(hsrc_ip, hdst_ip, env);
		}
	}
}

#if 1
void init_f(struct map *env)
{
	fprintf(stderr, "f init\n");
	map_add_key(env, PTR("group_table"), PTR(igmp_init()),
		    mapf_eq_map, mapf_free_map);
}

struct route *f(struct packet *pkt, struct map *env, struct entity *me, int in_port)
{
	/* inspect L2 header */
	pull_header(pkt);

	/* call handler */
	if (strcmp(read_header_type(pkt), "sdnp") == 0) {
		return handle_sdnp(pkt, env);
	} else if(strcmp(read_header_type(pkt), "ipv4") == 0) {
		return handle_ipv4(pkt, env);
	} else {
		fprintf(stderr, "unknown protocol: %s.\n", read_header_type(pkt));
		return route();
	}
}

#else
void init_f(struct map *env)
{
	fprintf(stderr, "f init\n");
	map_add_key(env, PTR("table"),
		    PTR(map(mapf_eq_int, mapf_hash_int,
			    mapf_dup_int, mapf_free_int)),
		    mapf_eq_map, mapf_free_map);
}

static struct route *forward(struct entity *esw, int in_port, int out_port)
{
	struct route *r = route();
	route_add_edge(r, edge(NULL, 0, esw, in_port));
	route_add_edge(r, edge(esw, out_port, NULL, 0));
	return r;
}

static struct route *flood(struct entity *esw, int in_port)
{
	int i;
	struct route *r = route();
	route_add_edge(r, edge(NULL, 0, esw, in_port));
	for(i = 1; i <= 4; i++) {
		if(i == in_port)
			continue;
		route_add_edge(r, edge(esw, i, NULL, 0));
	}
	return r;
}

struct route *f(struct packet *pkt, struct map *env, struct entity *me, int in_port)
{
	int id = get_switch_dpid(me);
	uint64_t dst = value_to_48(read_packet(pkt, "dl_dst"));
	uint64_t src = value_to_48(read_packet(pkt, "dl_src"));
	int out_port;
	struct map *table, *ftable;

	table = map_read(env, PTR("table")).p;
	assert(table);

	ftable = map_read(table, INT(id)).p;
	if(ftable == NULL) {
		ftable = map(mapf_eq_int, mapf_hash_int,
			     mapf_dup_int, mapf_free_int);
		map_add_key(table, INT(id), PTR(ftable),
			    mapf_eq_map, mapf_free_map);
	}

	map_mod2(ftable, INT(src), INT(in_port), mapf_eq_int, mapf_free_int);

	out_port = map_read(ftable, INT(dst)).v;

	if(out_port != 0)
		return forward(me, in_port, out_port);
	else
		return flood(me, in_port);
}

#endif
