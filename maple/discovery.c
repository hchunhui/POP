#include "discovery.h"
// install flow table
// construct LLDP packet
// parse LLDP packet
// send LLDP packet

route_t links = {0};

int 
lldp_flow_install(struct xswitch *sw, int prio)
{
        struct match *ma;
        struct msgbuf *msg;
        struct action *ac;
        int idx;
        ma = match();
        idx = flow_table_get_field_index(sw->table0, "dl_type");
        if(idx >= 0)
                match_add(ma, idx, value_from_16(LLDP_TYPE), value_from_16(0xffff));
        ac = action();
        action_add(ac, AC_PACKET_IN, 0); 
        msg = msg_flow_entry_add(sw->table0, prio, ma, ac);
        match_free(ma);
        action_free(ac);
        xswitch_send(sw, msg);
        return prio;
}

uint8_t * 
lldp_pkt_construct(dpid_t dpid, port_t port, int *len/*OUT*/);
void
lldp_packet_send(struct xswitch *sw)
{
	uint8_t *pkt;
	int pkt_len;
	struct action *out_a;
	struct msgbuf *mb;
	uint16_t i;
	// TODO port number 
	for(i = 1; i <= sw->n_ports; i++)
	{
		out_a = action();
		action_add(out_a, AC_OUTPUT, i);
		pkt = lldp_pkt_construct(sw->dpid, i, &pkt_len);
		mb = msg_packet_out(0, pkt, pkt_len, out_a);
		action_free(out_a);
		free(pkt);
		xswitch_send(sw, mb);
	}
}
uint8_t * 
lldp_pkt_construct(dpid_t dpid, port_t port, int *len/*OUT*/)
{
	uint8_t *pkt;
	int i;

	// mac_head, chassis_id, port_id, ttl, end
	*len = 14 +3+5+sizeof(dpid_t) +3+sizeof(port_t) +4 +2;
	pkt = malloc(*len);

	// mac_header
	for (i=0; i<6; i++)
	{
		pkt[i] = 0;
		pkt[6+i] = LLDP_DST >> (40-8*i);
	}
	for (i=0; i<2; i++)
		pkt[12+i] = LLDP_TYPE >> (8-8*i);
	
	// chassis_id_tlv
	i = 14;
	pkt[i++] = ((CHASSIS_ID_TLV << 9 | (8+sizeof(dpid_t))) >> 8) & 0xff;
	pkt[i++] = (8+sizeof(dpid_t)) & 0xff;
	pkt[i++] = SUB_LOCAL;
	pkt[i++] = 'd';
	pkt[i++] = 'p';
	pkt[i++] = 'i';
	pkt[i++] = 'd';
	pkt[i++] = ':';
	// host endian
	memcpy(&pkt[i], &dpid, sizeof(dpid_t));
	i += sizeof(dpid_t);

	// port_id_tlv
	pkt[i++] = ((PORT_ID_TLV << 9 | (3+sizeof(port_t))) >> 8) & 0xff;
	pkt[i++] = (3+sizeof(port_t)) & 0xff;
	pkt[i++] = PORT_SUB_PORT;
	// host endian
	memcpy(&pkt[i], &port, sizeof(port_t));
	i += sizeof(port_t);

	// ttl_tlv, network endian
	pkt[i++] = (TTL_TLV << 1) & 0xff;
	pkt[i++] = 2 & 0xff;
	pkt[i++] = 0;
	pkt[i++] = 120 & 0xff;

	// end_tlv
	pkt[i++] = END_TLV << 1;
	pkt[i] = 0;
	return pkt;
}

void 
entities_update_old_link(edge_t *link)
{
	// TODO realize modify topo;
	dpid_t dpid1 = link->dpid1;
	dpid_t dpid2 = link->dpid2;
	uint16_t port1 = link->port1;
	uint16_t port2 = link->port2;
	struct entity *e1 = topo_get_switch(dpid1);
	struct entity *e2 = topo_get_switch(dpid2);
	entity_add_link(e1, port1, e2, port2);
}
void
entities_add_new_link(route_t *route, edge_t *link)
{
	dpid_t dpid1 = link->dpid1;
	dpid_t dpid2 = link->dpid2;
	uint16_t port1 = link->port1;
	uint16_t port2 = link->port2;
	struct entity *e1 = topo_get_switch(dpid1);
	struct entity *e2 = topo_get_switch(dpid2);
	entity_add_link(e1, port1, e2, port2);
}

// return length of the tlv
uint16_t
lldp_tlv_next(const uint8_t *packet, struct lldp_tlv *tlv/*OUT*/, uint16_t offset)
{
	tlv->type = value_to_8(value_extract(packet, offset, 7));
	tlv->length = value_to_16(value_extract(packet, offset+7, 9));
	assert((tlv->type < 9 && tlv->type >=0) || tlv->type == 127);
	assert(tlv->length >= 0 && tlv->length < 512);
	memcpy(tlv->value, packet + offset + 2, tlv->length);
	return tlv->length + 2;
}
dpid_t
chassis_id_tlv_parse_dpid(struct lldp_tlv *tlv)
{
	if(tlv->type != CHASSIS_ID_TLV)
		return 0;
	if(tlv->length < 6 + sizeof(dpid_t))
		return 0;
	dpid_t dpid = 0;
	uint8_t chassis_id_subtype = tlv->value[0];
	if (chassis_id_subtype != SUB_LOCAL)
		return 0;
	if (tlv->value[1] == 'd' && tlv->value[2] == 'p'
		&& tlv->value[3] == 'i' && tlv->value[4] == 'd'
		&& tlv->value[5] == ':') {
		memcpy(&dpid, &tlv->value[6], sizeof(dpid_t));
	}
	return dpid;
}
port_t
port_id_tlv_parse_port(struct lldp_tlv *tlv)
{
	if(tlv->type != PORT_ID_TLV)
		return 0;
	if(tlv->length < 3)
		return 0;
	uint16_t port;
	uint8_t port_id_subtype = tlv->value[0];
	if(port_id_subtype != PORT_SUB_PORT)
		return 0;
	memcpy(&port, &tlv->value[1], sizeof(port_t));
	return port;
}
bool
parse_lldp(const uint8_t *packet, edge_t *link/*OUT*/)
{
	struct lldp_tlv tlv;
	uint16_t offset = 14*8;
	uint8_t type = 1;
	dpid_t dpid = 0;
	uint16_t port = 0;
	uint8_t i = 0;
	while(type != 0){
		offset += lldp_tlv_next(packet, &tlv, offset);
		type = lldp_tlv_get_type(&tlv);
		if (type == CHASSIS_ID_TLV){
			if (i != 0)
				return false;
			dpid = chassis_id_tlv_parse_dpid(&tlv);
		} else if (type == PORT_ID_TLV) {
			if (i != 1)
				return false;
			port = port_id_tlv_parse_port(&tlv);
		} else if (type == TTL_TLV) {
			if (i != 2)
				return false;
		} else if ( i > 10) {
			return false;
		}
		i++;
	}
	link->dpid1 = dpid;
	link->port1 = port;
	return true;
}
// struct packet_in;
int
handle_lldp_packet_in(const struct packet_in *packet_in)
{
	if (links.total_size == 0)
		route_init(&links);
	const uint8_t *packet = packet_in_get_packet(packet_in);
	uint16_t length = packet_in_get_length(packet_in);
	if (length < 14 + 14)
		return -1;
	uint16_t port = packet_in_get_port(packet_in);
	dpid_t dpid = packet_in_get_dpid(packet_in);
	uint16_t eth_type = value_to_16(value_extract(packet, 96, 16));
	if (eth_type != LLDP_TYPE)
		return -2;
	edge_t link;
	link.dpid2 = dpid;
	link.port2 = port;
	if (! parse_lldp(packet, &link))
		return -3;
	// route_add_edge(&links, &link);
	if (! edge_valid(&link))
		return -4;
	uint8_t retval = route_update_edge(&links, &link);
	switch (retval) {
		case 0:
			break;
		case 1:
			entities_update_old_link(&link);
			break;
		case 2:
			entities_add_new_link(&links, &link);
			break;
		default:;
	}
	return 0;
}
