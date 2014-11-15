#ifndef _MAPLE_API_H_
#define _MAPLE_API_H_
#include "types.h"

struct route;
struct route *route(void);
void route_free(struct route *r);
void route_add_edge(struct route *r, dpid_t dpid1, int out_port, dpid_t dpid2, int in_port);
void route_union(struct route *r1, struct route *r2);

struct packet;
void pull_header(struct packet *pkt);
const char *read_header_type(struct packet *pkt);
value_t read_packet(struct packet *pkt, const char *field);
bool test_equal(struct packet *pkt, const char *field, value_t value);
const uint8_t *read_payload(struct packet *pkt, int *length);

void record(const char *name);
void invalidate(const char *name);

#endif /* _MAPLE_API_H_ */
