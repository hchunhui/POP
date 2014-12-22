#ifndef __DISCOVERY_H
#define __DISCOVERY_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "entity.h"
#include "edge.h"
#include "value.h"
#include "types.h"
#include "topo.h"
#include "lldp.h"
#include "xswitch/xswitch.h"

#define ETHERTYPE_IP 0x0800
#define ETHERTYPE_ARP 0x0806
#define LLDP_DST 0x0180C200000E
#define LLDP_TYPE 0x88CC

void
lldp_packet_send(struct xswitch *sw);
int
handle_topo_packet_in(struct xswitch *sw, int port, const uint8_t *packet, int length);

#endif
