#ifndef __DISCOVERY_H
#define __DISCOVERY_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "entity.h"
#include "link.h"
#include "value.h"
#include "types.h"
#include "topo.h"
#include "packet_in.h"
#include "lldp.h"
#include "xswitch.h"
#include "xswitch-private.h"

#define LLDP_DST 0x0180C200000E
#define LLDP_TYPE 0x88CC

int 
lldp_flow_install(struct xswitch *sw, int prio);
void
lldp_packet_send(struct xswitch *sw);
int
handle_lldp_packet_in(const struct packet_in *packet_in);

#endif
