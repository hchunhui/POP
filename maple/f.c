#include <stdlib.h>
#include <assert.h>
#include "types.h"
#include "maple_api.h"
#include "topo.h"
#include "entity.h"

struct nodeinfo
{
	int parent;
	int parent_out_port;
	int in_port;
};

// simple int queue;
struct queue {
	int *queue;
	int head, tail;
};

static struct queue *queue_init(int length)
{
	struct queue *q = malloc(sizeof(struct queue));
	q->queue = malloc(sizeof(int) * length);
	q->head = 0;
	q->tail = 0;
	return q;
}

static void enqueue(struct queue *q, int elem)
{
	q->queue[q->tail] = elem;
	q->tail++;
}

static int dequeue(struct queue *q)
{
	q->head++;
	return q->queue[q->head-1];
}

static int queue_empty(struct queue *q)
{
	return q->head == q->tail;
}

static void queue_free(struct queue *q)
{
	free(q->queue);
	free(q);
}

static int find_index(struct entity **es, int num, struct entity *e)
{
	int i;
	for(i = 0; i < num; i++)
		if(es[i] == e)
			return i;
	return -1;
}

static struct route *get_route(struct entity *dst, int dst_port,
			       struct nodeinfo *visited,
			       struct entity **switches, int switches_num)
{
	struct route *r = route();
	int head, second;
	dpid_t head_dpid, second_dpid;
	second = find_index(switches, switches_num, dst);
	head = visited[second].parent;
	second_dpid = entity_get_dpid(switches[second]);
	route_add_edge(r, second_dpid, dst_port, 0, 0);

	while(head >= 0)
	{
		head_dpid = entity_get_dpid(switches[head]);
		route_add_edge(r,
			       head_dpid,
			       visited[second].parent_out_port,
			       second_dpid,
			       visited[second].in_port);
		second_dpid = head_dpid;
		second = head;
		head = visited[second].parent;
	}

	route_add_edge(r, 0, 0, second_dpid, visited[second].in_port);
	return r;
}

static struct nodeinfo *get_tree(struct entity *src, int src_port,
				 struct entity **switches, int switches_num)
{
	struct nodeinfo *visited = malloc(sizeof(struct nodeinfo)*switches_num);
	struct queue *q = queue_init(switches_num);
	const struct entity_adj *adjs;
	int num_adjs;

	int i;
	int epos1, epos2;
	struct entity *entity1, *entity2;

	entity1 = src;
	epos1 = find_index(switches, switches_num, entity1);
	assert(epos1 != -1);

	for (i = 0; i < switches_num; i++)
		visited[i].parent = -1;
	visited[epos1].parent = -2;
	visited[epos1].in_port = src_port;
	enqueue(q, epos1);

	while (!queue_empty(q))
	{
		epos1 = dequeue(q);
		entity1 = switches[epos1];
		adjs = entity_get_adjs(entity1, &num_adjs);
		for (i = 0; i < num_adjs; i++)
		{
			entity2 = adjs[i].adj_entity;
			if(entity_get_type(entity2) != ENTITY_TYPE_SWITCH)
				continue;
			epos2 = find_index(switches, switches_num, entity2);
			assert(epos2 != -1);
			if (visited[epos2].parent == -1)
			{
				enqueue(q, epos2);
				visited[epos2].parent = epos1;
				visited[epos2].parent_out_port = adjs[i].out_port;
				visited[epos2].in_port = adjs[i].adj_in_port;
			}
		}
	}

	queue_free(q);
	return visited;
}

static void get_switch(struct entity *host, struct entity **sw, int *port)
{
	const struct entity_adj *adjs;
	int num_adjs;
	adjs = entity_get_adjs(host, &num_adjs);
	*sw = adjs->adj_entity;
	*port = adjs->adj_in_port;
}

struct route *f(struct packet *pkt)
{
	int switches_num;
	struct route *r, *rx;
	struct entity *src, *dst1, *dst2;
	int src_port, dst1_port, dst2_port;
	struct nodeinfo *visited;
	/* inspect packet */
	struct entity *hsrc = topo_get_host(read_packet(pkt, "dl_src"));
	struct entity *hdst1 = topo_get_host(read_packet(pkt, "dl_dst"));
	struct entity *hdst2 = topo_get_host(value_from_48(3));
	struct entity **switches = topo_get_switches(&switches_num);
	assert(hsrc && hdst1 && hdst2);

	/* find connected switch */
	get_switch(hsrc, &src, &src_port);
	get_switch(hdst1, &dst1, &dst1_port);
	get_switch(hdst2, &dst2, &dst2_port);

	/* calculate spanning tree */
	visited = get_tree(src, src_port, switches, switches_num);

	/* get route */
	r = route();
	rx = get_route(dst1, dst1_port, visited, switches, switches_num);
	route_union(r, rx);
	route_free(rx);
	if(src != dst2) {
		rx = get_route(dst2, dst2_port, visited, switches, switches_num);
		route_union(r, rx);
		route_free(rx);
	}
	free(visited);
	return r;
}
