#ifndef _ROUTE_PRIVATE_H_
#define _ROUTE_PRIVATE_H_
#include "route.h"

struct entity;
struct route
{
	int num_edges;
	edge_t edges[32];
};

#endif /* _ROUTE_PRIVATE_H_ */
