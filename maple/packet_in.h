#ifndef __PACKET_IN_H
#define __PACKET_IN_H
#include "types.h"

struct packet_in {
	const uint8_t *packet;
	uint16_t length;
	uint16_t port;
	dpid_t dpid;
};
const uint8_t *
packet_in_get_packet(const struct packet_in *p)
{
	return p->packet;
}
uint16_t
packet_in_get_length(const struct packet_in *p)
{
	return p->length;
}
uint16_t 
packet_in_get_port(const struct packet_in *p)
{
	return p->port;
}
dpid_t
packet_in_get_dpid(const struct packet_in *p)
{
	return p->dpid;
}

#endif
