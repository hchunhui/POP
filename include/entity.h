#ifndef _ENTITY_H_
#define _ENTITY_H_

#include "types.h"

struct entity;
enum entity_type { ENTITY_TYPE_HOST, ENTITY_TYPE_SWITCH };
struct entity_adj
{
	int out_port;
	int adj_in_port;
	struct entity *adj_entity;
};

enum entity_type entity_get_type(struct entity *e);
dpid_t entity_get_dpid(struct entity *e);
struct entity *entity_host_get_adj_switch(struct entity *e, int *sw_port);
const struct entity_adj *entity_get_adjs(struct entity *e, int *pnum);
void entity_print(struct entity *e);

#endif /* _ENTITY_H_ */
