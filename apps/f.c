#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "maple_api.h"
#include "route.h"

#include "igmp.h"
#include "spanning_tree.h"

static struct route *handle_sdnp(struct packet *pkt)
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

static struct route *handle_ipv4_unicast(uint32_t hsrc_ip, uint32_t hdst_ip)
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

static struct route *handle_ipv4_multicast(uint32_t hsrc_ip, uint32_t hdst_ip)
{
	int i;
	int switches_num;
	struct entity **switches = get_switches(&switches_num);
	struct route *rx, *r;
	struct entity *hsrc, *hdst;
	struct entity *src, *dst;
	int src_port, dst_port;
	struct nodeinfo *visited;

	uint32_t groupid = hdst_ip;
	int ngroup_member = get_origin_len(groupid);
	uint32_t *buffer = malloc(sizeof(uint32_t) * ngroup_member);
	get_group_maddrs(groupid, buffer, ngroup_member);
	r = route();

	hsrc = get_host_by_paddr(hsrc_ip);
	if(hsrc == NULL) {
		fprintf(stderr, "ipv4_multicast: bad src address.\n");
		return route();
	}
	src = get_host_adj_switch(hsrc, &src_port);

	visited = get_tree(src, src_port, NULL, switches, switches_num);
	for(i = 0; i < ngroup_member; i++) {
		hdst = get_host_by_paddr(buffer[i]);
		if(hdst == NULL) {
			fprintf(stderr, "ipv4_multicast: bad dst address.\n");
			continue;
		}

		dst = get_host_adj_switch(hdst, &dst_port);
		rx = get_route(dst, dst_port, visited, switches, switches_num);
		route_union(r, rx);
		route_free(rx);
	}
	free(buffer);
	free(visited);
	return r;
}

static struct route *handle_igmp(struct packet *pkt)
{
	int len;
	uint32_t hsrc_ip = value_to_32(read_packet(pkt, "nw_src"));
	const uint8_t *payload = read_payload(pkt, &len);
	process_igmp(hsrc_ip, payload, len);
	return route();
}

static bool is_multicast_ip(uint32_t ip)
{
	if((ip >> 24) >= 224 && (ip >> 24) < 240)
		return true;
	return false;
}

static struct route *handle_ipv4(struct packet *pkt)
{
	if(test_equal(pkt, "nw_proto", value_from_8(0x02))) {
		return handle_igmp(pkt);
	} else {
		uint32_t hsrc_ip = value_to_32(read_packet(pkt, "nw_src"));
		uint32_t hdst_ip = value_to_32(read_packet(pkt, "nw_dst"));
		if(is_multicast_ip(hdst_ip)) {
			return handle_ipv4_multicast(hsrc_ip, hdst_ip);
		} else {
			return handle_ipv4_unicast(hsrc_ip, hdst_ip);
		}
	}
}


void init_f(void)
{
	fprintf(stderr, "f init\n");
}

struct route *f(struct packet *pkt)
{
	/* inspect L2 header */
	pull_header(pkt);

	/* call handler */
	if (strcmp(read_header_type(pkt), "sdnp") == 0) {
		return handle_sdnp(pkt);
	} else if(strcmp(read_header_type(pkt), "ipv4") == 0) {
		return handle_ipv4(pkt);
	} else {
		fprintf(stderr, "unknown protocol: %s.\n", read_header_type(pkt));
		return route();
	}
}
