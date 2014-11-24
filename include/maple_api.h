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

#endif /* _MAPLE_API_H_ */
