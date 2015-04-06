#ifndef _SPANNING_TREE_H_
#define _SPANNING_TREE_H_

struct entity;
struct route;
struct nodeinfo;

struct nodeinfo *get_tree(struct entity *src, int src_port, struct entity *dst,
			  struct entity **switches, int switches_num);

struct route *get_route(struct entity *dst, int dst_port,
			struct nodeinfo *visited,
			struct entity **switches, int switches_num);

#endif /* _SPANNING_TREE_H_ */
