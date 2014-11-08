#include <stdlib.h>
#include <assert.h>
#include "entity.h"
#include "xswitch-private.h"

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
		printf("\nHOST:\nEth Addr: ");
		for (i=0; i<6; i++)
			printf("%02x ",e->u.addr.haddr.octet[i]);
		printf("\nIPv4: %08x\n", e->u.addr.paddr);
		printf("num_adjs: %d\n", e->num_adjs);
		for (i=0; i<e->num_adjs; i++) {
			printf("  %3d: %d, %d, %d\n",i, e->adjs[i].out_port, e->adjs[i].adj_in_port, (e->adjs[i].adj_entity)->u.xs->dpid);
		}
	} else if (e->type == ENTITY_TYPE_SWITCH) {
		printf("\nSWITCH:\nDpid: %d\n", e->u.xs->dpid);
		printf("num_adjs: %d\n", e->num_adjs);
		for (i=0; i<e->num_adjs; i++) {
			printf("  %3d: %d, %d, %d\n",i, e->adjs[i].out_port, e->adjs[i].adj_in_port, (e->adjs[i].adj_entity)->type);
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
/*
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
	*/
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

struct host_info entity_get_addr(struct entity *e)
{
	assert(e->type == ENTITY_TYPE_HOST);
	return e->u.addr;
}
struct entity *entity_host_get_adj_switch(struct entity *e, port_t *sw_port)
{
//	printf("e: %p  sw_port: %p\n", e, sw_port);
        if (e->type != ENTITY_TYPE_HOST)
                return NULL;
        *sw_port = e->adjs[0].adj_in_port;
//      printf("e %p\n", e->adjs[0].adj_entity);
        return e->adjs[0].adj_entity;
}
void entity_port_down(struct entity *e, port_t port)
{
        int i;
        if (e->type == ENTITY_TYPE_HOST)
                return;
        for (i = 0; i < e->num_adjs; i++) {
                if (e->adjs[i].out_port == port) {
                        e->num_adjs --;
                        if (i != e->num_adjs)
                                e->adjs[i] = e->adjs[e->num_adjs];
                        e->adjs[e->num_adjs].out_port = 0;
                        e->adjs[e->num_adjs].adj_in_port = 0;
                        e->adjs[e->num_adjs].adj_entity = NULL;
                }
        }
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
