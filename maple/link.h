#ifndef __TOPO_H
#define __TOPO_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
typedef uint16_t port_t;

typedef struct 
{
	dpid_t dpid1;
	dpid_t dpid2;
	int port1;
	int port2;
} edge_t;

typedef struct
{
	edge_t *rt;
	int len;
	int total_size;
} route_t;

/*
enum entity_type {HOST, SW};

typedef struct entity 
{
	enum entity_type type;
	union 
	{
		dpid_t dpid;
		char addr[6];
	};
	int adjs_num;
	struct 
	{
		int out_port;
		int adj_in_port;
		struct entity *adj_entity;
	} *adjs;
}entity_t;
dpid_t 
entity_get_dpid(struct entity *e)
{
	assert(e->type == SW);
	return e->dpid;
}
// char (*)[6]
char * 
entity_get_addr(struct entity *e, int *len )
{
	assert(e->type == HOST);
	*len = sizeof(e->addr);
	return e->addr;
}

enum entity_type
entity_get_type(struct entity *e)
{
	return e->type;
}

int entity_get_adjs_num(struct entity *e)
{
	return e->adjs_num;
}
struct entity *
entity_get_adj_entity(struct entity *e, int i)
{
	assert(i < e->adjs_num && i >= 0);
	return e->adjs[i].adj_entity;
}
int entity_get_out_port(struct entity *e, int i)
{
	assert(i < e->adjs_num && i >= 0);
	return e->adjs[i].out_port;
}
int entity_get_adj_in_port(struct entity *e, int i)
{
	assert(i < e->adjs_num && i >= 0);
	return e->adjs[i].adj_in_port;
}
*/
static inline edge_t 
edge_new(dpid_t dpid1, int port1, dpid_t dpid2, int port2)
{
	edge_t e;
	e.dpid1 = dpid1;
	e.dpid2 = dpid2;
	e.port1 = port1;
	e.port2 = port2;
	return e;
}
static inline
int edge_equal(edge_t *e1, edge_t *e2)
{
	return e1->dpid1 == e2->dpid1 && e1->dpid2 == e2->dpid2 
		&& e1->port1 == e2->port1 && e1->port2 == e2->port2;
}
static inline
bool edge_valid(edge_t *e)
{
	return e->dpid1 != 0 && e->dpid2 != 0
		&& e->port1 != 0 && e->port2 != 0;
}
static inline
void route_init(route_t *route)
{
	route->len = 0;
	route->total_size = 20;
	route->rt = malloc(sizeof(edge_t)*20);
	assert(route->rt != NULL);
}
static inline
void route_init_size(route_t *route, int length)
{
	if (length <= 0)
		length = 20;
	route->len = 0;
	route->total_size = length;
	route->rt = malloc(sizeof(edge_t)*length);
	assert(route->rt != NULL);
}
static inline
void route_add_size(route_t *route)
{
	route->total_size <<= 1;
	route->rt = realloc(route->rt, sizeof(edge_t)*route->total_size);
	assert(route->rt != NULL);
}
static inline
void route_add_edge(route_t *route, edge_t *e)
{
	assert(route->len <= route->total_size);
	if (route->len == route->total_size)
		route_add_size(route);
	int i = 0; 
	for (;i < route->len; i ++)
	{
		if (edge_equal(&route->rt[i], e))
			break;
	}
	if (i == route->len)
	{
		route->len ++;
		route->rt[i] = *e;
	}
}
/*
   * return:
   * 0: edge existed already.
   * 1: update old link.
   * 2: add new link.
   */
static inline
uint8_t route_update_edge(route_t *route, edge_t *e)
{
	int i = 0;
	for (; i<route->len; i++) {
		if (route->rt[i].dpid1 == e->dpid1
			&& route->rt[i].dpid2 == e->dpid2) {
			if (route->rt[i].port1 == e->port1 
				&& route->rt[i].port2 == e->port2)
				return 0;
			else {
				route->rt[i].port1 = e->port1;
				route->rt[i].port2 = e->port2;
				return 1;
			}
		}
	}
	if (route->len == route->total_size)
		route_add_size(route);
	if ( i == route->len)
	{
		route->len ++;
		route->rt[i] = *e;
	}
	return 2;
}
static inline
void route_add_edges(route_t *route, edge_t *e, int len)
{
	int i;
	for (i=0; i<len; i++)
		route_add_edge(route, &e[i]);
}
static inline
void route_merge(route_t *rt1, route_t *rt2)
{
	route_add_edges(rt1, rt2->rt, rt2->len);
}
static inline
void route_print(route_t *r)
{
	int i;
	for (i = 0; i < r->len; i ++)
	{
		printf("(%d, %d, %d, %d)\n", r->rt[i].dpid1, 
			r->rt[i].port1, r->rt[i].port2, r->rt[i].dpid2);
	}
}
static inline
void route_destory(route_t *route)
{
	free(route->rt);
}
#endif
