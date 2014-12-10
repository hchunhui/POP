#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "maple_api.h"
#include "route.h"

#include "igmp.h"
#include "spanning_tree.h"

static bool is_multicast_ip(uint32_t ip)
{
	if((ip >> 24) >= 224 && (ip >> 24) < 240)
		return true;
	return false;
}

struct route *f(struct packet *pkt)
{
	int switches_num;
	struct entity **switches = get_switches(&switches_num);
	struct route *r;
	struct entity *src, *dst;
	int src_port, dst_port;
	struct nodeinfo *visited;
	uint32_t hdst_ip, hsrc_ip;
	struct entity *hsrc, *hdst;

	/* inspect packet */
	pull_header(pkt);

	if(strcmp(read_header_type(pkt), "ipv4") != 0)
		return route();

	hsrc_ip = value_to_32(read_packet(pkt, "nw_src"));
	hdst_ip = value_to_32(read_packet(pkt, "nw_dst"));

	if(test_equal(pkt, "nw_proto", value_from_8(0x02))) {
		int len;
		const uint8_t *payload = read_payload(pkt, &len);
		process_igmp(hsrc_ip, payload, len);
		return route();
	}

	hsrc = get_host_by_paddr(hsrc_ip);
	assert(hsrc);
	src = get_host_adj_switch(hsrc, &src_port);

	/* add routes */
	if(is_multicast_ip(hdst_ip)) {
		int i;
		struct route *rx;
		uint32_t groupid = hdst_ip;
		int ngroup_member = get_origin_len(groupid);
		uint32_t *buffer = malloc(sizeof(uint32_t) * ngroup_member);
		get_group_maddrs(groupid, buffer, ngroup_member);

		r = route();
		/* calculate spanning tree */
		visited = get_tree(src, src_port, NULL, switches, switches_num);
		for(i = 0; i < ngroup_member; i++){
			hdst = get_host_by_paddr(buffer[i]);
			if(hdst == NULL)
				continue;
			/* find connected switch */
			dst = get_host_adj_switch(hdst, &dst_port);
			/* get and union route */
			rx = get_route(dst, dst_port, visited, switches, switches_num);
			route_union(r, rx);
			route_free(rx);
		}
		free(buffer);
		free(visited);
	} else {
		hdst = get_host_by_paddr(hdst_ip);
		if(hdst == NULL) {
			fprintf(stderr, "dst host not found: %08x\n", hdst_ip);
			r = route();
		} else {
			/* find connected switch */
			dst = get_host_adj_switch(hdst, &dst_port);
			/* get route */
			visited = get_tree(src, src_port, dst, switches, switches_num);
			r = get_route(dst, dst_port, visited, switches, switches_num);
			free(visited);
		}
	}

	return r;
}
