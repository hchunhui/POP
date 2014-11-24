#include <stdio.h>
#include <stdlib.h>
#include "route-private.h"

struct route *route(void)
{
	struct route *r = malloc(sizeof(struct route));
	r->num_edges = 0;
	return r;
}

void route_free(struct route *r)
{
	free(r);
}

void route_add_edge(struct route *r, edge_t e)
{
	int i = r->num_edges;
	if(i >= 32) {
		fprintf(stderr, "route: too many hops!\n");
		return;
	}
	r->edges[i] = e;
	r->num_edges++;
}

void route_union(struct route *r1, struct route *r2)
{
	int i, j;
	for(j = 0; j < r2->num_edges; j++) {
		for(i = 0; i < r1->num_edges; i++) {
			if(edge_equal(r1->edges[i], r2->edges[j]))
				break;
		}
		if(i >= r1->num_edges)
			route_add_edge(r1, r2->edges[j]);
	}
}

edge_t *route_get_edges(struct route *r, int *num)
{
	*num = r->num_edges;
	return r->edges;
}
