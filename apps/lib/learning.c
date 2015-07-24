#include <assert.h>
#include "types.h"
#include "pop_api.h"
#include "route.h"
#include "map.h"

static struct route *forward(struct entity *esw, int in_port, int out_port)
{
	struct route *r = route();
	route_add_edge(r, edge(NULL, 0, esw, in_port));
	route_add_edge(r, edge(esw, out_port, NULL, 0));
	return r;
}

static struct route *flood(struct entity *esw, int in_port)
{
	int i, n;
	struct route *r = route();
	const struct entity_adj *adjs = get_entity_adjs(esw, &n);

	route_add_edge(r, edge(NULL, 0, esw, in_port));
	for(i = 0; i < n; i++) {
		int port = adjs[i].out_port;
		if(port != in_port) {
			route_add_edge(r, edge(esw, port, NULL, 0));
		}
	}
	return r;
}

struct route *learning(struct map *env, char *table_name,
		       struct entity *me, int in_port, uint64_t src,
		       uint64_t dst)
{
	int id = get_switch_dpid(me);
	int out_port;
	struct map *table, *ftable;

	table = map_read(env, PTR(table_name)).p;
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

void learning_init(struct map *env, char *table_name)
{
	map_add_key(env, PTR(table_name),
		    PTR(map(mapf_eq_int, mapf_hash_int,
			    mapf_dup_int, mapf_free_int)),
		    mapf_eq_map, mapf_free_map);
}
