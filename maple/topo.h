#ifndef _TOPO_H_
#define _TOPO_H_
#include "types.h"

struct xswitch;
void topo_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len);
void topo_switch_up(struct xswitch *sw);
void topo_switch_down(struct xswitch *sw);

struct entity **topo_get_hosts(int *pnum);
struct entity **topo_get_switches(int *pnum);
struct entity *topo_get_host(value_t addr);
struct entity *topo_get_switch(dpid_t dpid);
#endif
