#include <stdlib.h>
#include <assert.h>
#include "entity.h"
#include "xswitch-private.h"

struct entity
{
	enum entity_type type;
	union {
		struct xswitch *xs;
		value_t addr;
	} u;

	int num_adjs;
	struct entity_adj adjs[16];
};

struct entity *entity_host(value_t addr)
{
	struct entity *e = malloc(sizeof(struct entity));
	e->type = ENTITY_TYPE_HOST;
	e->u.addr = addr;
	e->num_adjs = 0;
	return e;
}

struct entity *entity_switch(struct xswitch *xs)
{
	struct entity *e = malloc(sizeof(struct entity));
	e->type = ENTITY_TYPE_SWITCH;
	e->u.xs = xs;
	e->num_adjs = 0;
	return e;
}

void entity_free(struct entity *e)
{
	int i, j;
	for(i = 0; i < e->num_adjs; i++) {
		struct entity *peer = e->adjs[i].adj_entity;
		int port = e->adjs[i].adj_in_port;
		for(j = 0; j < peer->num_adjs; j++) {
			if(peer->adjs[j].out_port == port &&
			   peer->adjs[j].adj_entity == e) {
				peer->num_adjs--;
				peer->adjs[j] = peer->adjs[peer->num_adjs];
				break;
			}
		}
	}
	free(e);
}

enum entity_type entity_get_type(struct entity *e)
{
	return e->type;
}

struct xswitch *entity_get_xswitch(struct entity *e)
{
	assert(e->type == ENTITY_TYPE_SWITCH);
	return e->u.xs;
}

/* hack */
dpid_t entity_get_dpid(struct entity *e)
{
	assert(e->type == ENTITY_TYPE_SWITCH);
	return e->u.xs->dpid;
}

value_t entity_get_addr(struct entity *e)
{
	assert(e->type == ENTITY_TYPE_HOST);
	return e->u.addr;
}

const struct entity_adj *entity_get_adjs(struct entity *e, int *pnum)
{
	*pnum = e->num_adjs;
	return e->adjs;
}

void entity_add_link(struct entity *e1, int port1, struct entity *e2, int port2)
{
	int i = e1->num_adjs;
	int j = e2->num_adjs;
	e1->adjs[i].out_port = port1;
	e1->adjs[i].adj_in_port = port2;
	e1->adjs[i].adj_entity = e2;
	e2->adjs[j].out_port = port2;
	e2->adjs[j].adj_in_port = port1;
	e2->adjs[j].adj_entity = e1;
	e1->num_adjs++;
	e2->num_adjs++;
}
