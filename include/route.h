#ifndef _ROUTE_H_
#define _ROUTE_H_
#include "edge.h"

struct entity;
struct route;
struct route *route(void);
void route_free(struct route *r);
void route_add_edge(struct route *r, edge_t e);
void route_union(struct route *r1, struct route *r2);
edge_t *route_get_edges(struct route *r, int *num);

#endif /* _ROUTE_H_ */
