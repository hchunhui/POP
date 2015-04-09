#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "maple_api.h"
#include "route.h"
#include "map.h"

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

	if(dst == 0xffffffffffffull)
		return flood(me, in_port);

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
