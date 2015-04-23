#ifndef _EDGE_H_
#define _EDGE_H_
#include "types.h"

typedef struct
{
	struct entity *ent1;
	int port1;
	struct entity *ent2;
	int port2;
} edge_t;

static inline edge_t edge(struct entity *ent1,
			  int port1,
			  struct entity *ent2,
			  int port2)
{
	edge_t e;
	e.ent1 = ent1;
	e.port1 = port1;
	e.ent2 = ent2;
	e.port2 = port2;
	return e;
}

static inline bool edge_equal(edge_t e1, edge_t e2)
{
	return
		e1.ent1 == e2.ent1 &&
		e1.port1 == e2.port1 &&
		e1.ent2 == e2.ent2 &&
		e1.port2 == e2.port2;
}

#endif /* _EDGE_H_ */
