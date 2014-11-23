#ifndef _TOPO_H_
#define _TOPO_H_
#include "types.h"

#define MAX_ENTITY_NUM 100

struct xswitch;
struct haddr;
bool topo_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len);
void topo_switch_up(struct xswitch *sw);
void topo_switch_down(struct xswitch *sw);
void topo_switch_port_down(struct xswitch *sw, int port);

struct entity **topo_get_hosts(int *pnum);
struct entity **topo_get_switches(int *pnum);
struct entity *topo_get_switch(dpid_t dpid);
struct entity *topo_get_host_by_haddr(haddr_t addr);
struct entity *topo_get_host_by_paddr(uint32_t addr);
int topo_add_host(struct entity *e);
int topo_add_switch(struct entity *e);
int topo_del_host(struct entity *e);
int topo_del_switch(struct entity *e);
void topo_print();
#endif
