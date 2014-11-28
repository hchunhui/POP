#ifndef _ROUTE_PRIVATE_H_
#define _ROUTE_PRIVATE_H_
#include "route.h"

#define MAX_NUM_EDGES 32
struct entity;
struct route
{
	int num_edges;
	edge_t edges[MAX_NUM_EDGES];
};

#endif /* _ROUTE_PRIVATE_H_ */
