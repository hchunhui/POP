#ifndef _MAPLE_API_H_
#define _MAPLE_API_H_
#include "types.h"

struct route;
struct route *route(void);
void route_free(struct route *r);
void route_add_edge(struct route *r, dpid_t dpid1, int out_port, dpid_t dpid2, int in_port);
void route_union(struct route *r1, struct route *r2);

struct packet;
value_t read_packet(struct packet *pkt, const char *field);
bool test_equal(struct packet *pkt, const char *field, value_t value);

struct env;
void *read_env(struct env *env, const char *name);
void write_env(struct env *env, const char *name, void *value);
void invalidate_env(struct env *env, const char *name);

#endif /* _MAPLE_API_H_ */
