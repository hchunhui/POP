#ifndef _SPANNING_TREE_H_
#define _SPANNING_TREE_H_

#include <stdlib.h>
#include <assert.h>
#include "types.h"
// #include "maple_api.h"
// #include "topo.h"
#include "entity.h"

struct nodeinfo
{
	int parent;
	int parent_out_port;
	int in_port;
};
struct nodeinfo *
spanning_tree_init(struct entity *src, int src_port, struct entity **switches, int switches_num);

#endif
