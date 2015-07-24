#include <stdlib.h>
#include <assert.h>
#include "types.h"
#include "pop_api.h"
#include "route.h"

struct nodeinfo
{
	int parent;
	int parent_out_port;
	int in_port;
};

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

struct route *get_route(struct entity *dst, int dst_port,
			struct nodeinfo *visited,
			struct entity **switches, int switches_num)
{
	struct route *r = route();
	int head, second;
	struct entity *head_e, *second_e;
	second = find_index(switches, switches_num, dst);
	head = visited[second].parent;
	second_e = switches[second];

	/* Destination unreachable? */
	if(head == -1)
		return r;

	route_add_edge(r, edge(second_e, dst_port, NULL, 0));

	while(head >= 0)
	{
		head_e = switches[head];
		route_add_edge(r, edge(
				       head_e,
				       visited[second].parent_out_port,
				       second_e,
				       visited[second].in_port));
		second_e = head_e;
		second = head;
		head = visited[second].parent;
	}

	route_add_edge(r, edge(NULL, 0, second_e, visited[second].in_port));
	return r;
}

struct nodeinfo *get_tree(struct entity *src, int src_port, struct entity *dst,
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
		if(entity1 == dst)
			break;
		adjs = get_entity_adjs(entity1, &num_adjs);
		for (i = 0; i < num_adjs; i++)
		{
			entity2 = adjs[i].adj_entity;
			if(get_entity_type(entity2) != ENTITY_TYPE_SWITCH)
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
