#include <stdlib.h>
#include <assert.h>
#include "xlog/xlog.h"
#include "types.h"
#include "pop_api.h"
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

	pull_header(pkt);
	src = get_switch(value_to_32(read_packet(pkt, "dpid_src")));
	dst = get_switch(value_to_32(read_packet(pkt, "dpid_dst")));
	src_port = value_to_16(read_packet(pkt, "port_src"));
	dst_port = value_to_16(read_packet(pkt, "port_dst"));

	if(src == NULL || dst == NULL) {
		xerror("sdnp: bad address.\n");
		return route();
	}

	visited = get_tree(src, src_port, dst, switches, switches_num);
	r = get_route(dst, dst_port, visited, switches, switches_num);
	free(visited);
	push_header(pkt);
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
		xerror("ipv4_unicast: bad address.\n");
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
		xerror("ipv4_multicast: bad src address.\n");
		return route();
	}
	src = get_host_adj_switch(hsrc, &src_port);

	visited = get_tree(src, src_port, NULL, switches, switches_num);
	xinfo("group_id: %08x, group_n: %d\n", hdst_ip, l->n);
	for(i = 0; i < l->n; i++) {
		hdst = get_host_by_paddr(l->addrs[i]);
		if(hdst == NULL) {
			xerror("ipv4_multicast: bad dst address.\n");
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
			mod_packet(pkt, "ttl", value_from_8(42));
			return handle_ipv4_unicast(hsrc_ip, hdst_ip, env);
		}
	}
}

void init_f(struct map *env)
{
	xinfo("f init\n");
	map_add_key(env, PTR("group_table"), PTR(igmp_init()),
		    mapf_eq_map, mapf_free_map);
}

struct route *f(struct packet *pkt, struct map *env)
{
	struct route *r = NULL;

	/* inspect network header */
	pull_header(pkt);

	/* call handler */
	if (strcmp(read_header_type(pkt), "sdnp") == 0) {
		r = handle_sdnp(pkt, env);
	} else if(strcmp(read_header_type(pkt), "ipv4") == 0) {
		r = handle_ipv4(pkt, env);
	} else {
		xinfo("unknown protocol: %s.\n", read_header_type(pkt));
		r = route();
	}

	/* reset header */
	push_header(pkt);

	return r;
}
