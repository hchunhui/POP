#ifndef _MAPLE_API_H_
#define _MAPLE_API_H_
#include "types.h"

struct route;
struct packet;
void pull_header(struct packet *pkt);
const char *read_header_type(struct packet *pkt);
value_t read_packet(struct packet *pkt, const char *field);
bool test_equal(struct packet *pkt, const char *field, value_t value);
const uint8_t *read_payload(struct packet *pkt, int *length);

void record(const char *name);
void invalidate(const char *name);

struct entity;
struct entity **get_hosts(int *pnum);
struct entity **get_switches(int *pnum);
struct entity *get_switch(dpid_t dpid);
struct entity *get_host_by_haddr(haddr_t addr);
struct entity *get_host_by_paddr(uint32_t addr);

#ifndef _ENTITY_H_
enum entity_type { ENTITY_TYPE_HOST, ENTITY_TYPE_SWITCH };
struct entity_adj
{
	int out_port;
	int adj_in_port;
	struct entity *adj_entity;
};
#endif

enum entity_type get_entity_type(struct entity *e);
dpid_t get_switch_dpid(struct entity *e);
struct entity *get_host_adj_switch(struct entity *e, int *sw_port);
const struct entity_adj *get_entity_adjs(struct entity *e, int *pnum);
void print_entity(struct entity *e);

#endif /* _MAPLE_API_H_ */
