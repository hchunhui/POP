#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "entity-private.h"
#include "xswitch/xswitch.h"

struct entity
{
	enum entity_type type;
	union {
		struct xswitch *xs;
		struct host_info addr;
	} u;

	// int num_ports;
	int num_adjs;
	struct entity_adj adjs[MAX_PORT_NUM];
};

void entity_print(struct entity *e)
{
	int i;
	if (e->type == ENTITY_TYPE_HOST) {
		fprintf(stderr, "HOST:\nEth Addr: ");
		for (i=0; i<6; i++)
			fprintf(stderr, "%02x ",e->u.addr.haddr.octet[i]);
		fprintf(stderr, "\nIPv4: %08x\n", e->u.addr.paddr);
		fprintf(stderr, "num_adjs: %d\n", e->num_adjs);
		for (i=0; i<e->num_adjs; i++) {
			fprintf(stderr, "  %3d: %d, %d, %d\n", i,
				e->adjs[i].out_port, e->adjs[i].adj_in_port,
				entity_get_dpid(e->adjs[i].adj_entity));
		}
	} else if (e->type == ENTITY_TYPE_SWITCH) {
		fprintf(stderr, "SWITCH:\nDpid: %d\n", entity_get_dpid(e));
		fprintf(stderr, "num_adjs: %d\n", e->num_adjs);
		for (i=0; i<e->num_adjs; i++) {
			fprintf(stderr, "  %3d: %d, %d, %d\n", i,
				e->adjs[i].out_port, e->adjs[i].adj_in_port,
				(e->adjs[i].adj_entity)->type);
		}
	}
}

struct entity *entity_host(struct host_info addr)
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

dpid_t entity_get_dpid(struct entity *e)
{
	if(e) {
		assert(e->type == ENTITY_TYPE_SWITCH);
		return xswitch_get_dpid(e->u.xs);
	} else {
		return 0;
	}
}

struct host_info entity_get_addr(struct entity *e)
{
	assert(e->type == ENTITY_TYPE_HOST);
	return e->u.addr;
}

void entity_set_paddr(struct entity *e, uint32_t paddr)
{
	assert(e->type == ENTITY_TYPE_HOST);
	e->u.addr.paddr = paddr;
}

struct entity *entity_host_get_adj_switch(struct entity *e, int *sw_port)
{
        if (e->type != ENTITY_TYPE_HOST)
                return NULL;
        *sw_port = e->adjs[0].adj_in_port;
        return e->adjs[0].adj_entity;
}

const struct entity_adj *entity_get_adjs(struct entity *e, int *pnum)
{
	*pnum = e->num_adjs;
	return e->adjs;
}

void entity_add_link(struct entity *e1, int port1, struct entity *e2, int port2)
{
	int k, i, j;
	for (k = 0; k < e1->num_adjs; k++) {
		if (e1->adjs[k].out_port == port1 
		    && e1->adjs[k].adj_in_port == port2
		    && e1->adjs[k].adj_entity == e2)
			return;
	}
	i = e1->num_adjs;
	j = e2->num_adjs;
	e1->adjs[i].out_port = port1;
	e1->adjs[i].adj_in_port = port2;
	e1->adjs[i].adj_entity = e2;
	e2->adjs[j].out_port = port2;
	e2->adjs[j].adj_in_port = port1;
	e2->adjs[j].adj_entity = e1;
	e1->num_adjs++;
	e2->num_adjs++;
}
