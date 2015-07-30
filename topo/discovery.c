#include <pthread.h>
#include <string.h>
#include "discovery.h"
// install flow table
// construct LLDP packet
// parse LLDP packet
// send LLDP packet

static pthread_mutex_t arp_lock = PTHREAD_MUTEX_INITIALIZER;
static struct entity *arp_waiting_hosts[20];
static int next_available_waiting_host;

static inline void get_next_available_waiting_host()
{
	int i;
	for (i = 0; i < 20; i++)
		if (arp_waiting_hosts[i] == NULL)
			break;
	next_available_waiting_host = i;
}

static void port_packet_out(struct xswitch *xsw, int port, const uint8_t *pkt, int len)
{
	struct action *ac;
	struct msgbuf *mb;
	ac = action();
	action_add(ac, AC_OUTPUT, port);
	mb = msg_packet_out(0, pkt, len, ac);
	action_free(ac);
	xswitch_send(xsw, mb);
}

static uint8_t * 
lldp_pkt_construct(dpid_t dpid, port_t port, int *len/*OUT*/);

void
lldp_packet_send(struct xswitch *sw)
{
	uint8_t *pkt;
	int pkt_len;
	int i;
	// TODO port number
	int n_ports = xswitch_get_num_ports(sw);
	for(i = 1; i <= n_ports; i++)
	{
		pkt = lldp_pkt_construct(xswitch_get_dpid(sw), i, &pkt_len);
		// int to port_t conversion
		port_packet_out(sw, i, pkt, pkt_len);
		free(pkt);
	}
}

static uint8_t * 
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
	pkt[i++] = ((CHASSIS_ID_TLV << 9 | (6+sizeof(dpid_t))) >> 8) & 0xff;
	pkt[i++] = (6+sizeof(dpid_t)) & 0xff;
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
	pkt[i++] = ((PORT_ID_TLV << 9 | (1+sizeof(port_t))) >> 8) & 0xff;
	pkt[i++] = (1+sizeof(port_t)) & 0xff;
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

// return length of the tlv
static uint16_t
lldp_tlv_next(const uint8_t *packet, struct lldp_tlv *tlv/*OUT*/, uint16_t offset)
{
	uint16_t v = value_to_16(value_extract(packet, offset, 16));
        tlv->type = v >> 9;
        tlv->length = v & 511;
        assert((tlv->type < 9 && tlv->type >=0) || tlv->type == 127);
        assert(tlv->length < 512);
        memcpy(tlv->value, packet + offset/8 + 2, tlv->length);
        return tlv->length + 2;
}

static dpid_t
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

static port_t
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

static bool
parse_lldp(const uint8_t *packet, edge_t *link/*OUT*/)
{
	struct lldp_tlv tlv;
	uint16_t offset = 14*8;
	uint8_t type = 1;
	dpid_t dpid = 0;
	uint16_t port = 0;
	uint8_t i = 0;
	while(type != 0){
		offset += 8*lldp_tlv_next(packet, &tlv, offset);
		type = lldp_tlv_get_type(&tlv);
		fprintf(stderr, "type: %d\n", type);
		if (type == CHASSIS_ID_TLV){
			if (i != 0)
			{
				return false;
			}
			dpid = chassis_id_tlv_parse_dpid(&tlv);
		} else if (type == PORT_ID_TLV) {
			if (i != 1)
			{
				return false;
			}
			port = port_id_tlv_parse_port(&tlv);
		} else if (type == TTL_TLV) {
			if (i != 2)
			{
				return false;
			}
		} else if ( i > 10) {
			return false;
		}
		i++;
	}
	link->ent1 = topo_get_switch(dpid);
	// port_t to int conversion
	link->port1 = port;
	return true;
}

static int
handle_lldp_packet_in(const uint8_t *packet, int length, struct xswitch *sw, int port)
{
	if (length < 14 + 14)
	{
		fprintf(stderr, "length < 28\n");
		return -1;
	}
	uint16_t eth_type = value_to_16(value_extract(packet, 96, 16));
	if (eth_type != LLDP_TYPE)
		return -2;
	edge_t link;
	topo_wrlock();
	link.ent2 = topo_get_switch(xswitch_get_dpid(sw));
	link.port2 = port;
	if (! parse_lldp(packet, &link))
	{
		fprintf(stderr, "parse_lldp error\n");
		topo_unlock();
		return -3;
	}
	entity_add_link(link.ent1, link.port1, link.ent2, link.port2);
	topo_unlock();
	return 0;
}

struct arp_header {
	uint16_t arp_hrd;
	uint16_t arp_pro;
	uint8_t arp_hln;
	uint8_t arp_pln;
	uint16_t arp_op;
#define ARPOP_REQUEST	1
#define ARPOP_REPLY	2
#define ARPOP_REVREQUEST 3
#define ARPOP_REVREPLY	4

	haddr_t arp_sha;
	uint32_t arp_spa;
	haddr_t arp_tha;
	uint32_t arp_tpa;

};

/*
arp   1    2    3    4
14   ---- ---- ---- ----
     hrd       pro
18   ---- ---- ---- ----
     hln  pln  op
22   ---- ---- ---- ----
     sha
26   ---- ---- ---- ----
               spa
30   ---- ---- ---- ----
               tha
34   ---- ---- ---- ----

38   ---- ---- ---- ----
     tpa
42   ---- ---- ---- ----
*/

static uint8_t*
arp_reply_packet_construct(struct host_info *hinfo, struct arp_header *arp, int *len /*OUT*/)
{
	*len = 14+28;
	uint8_t *pkt = malloc(*len);
	int i;
	for (i = 0; i < 6; i++) {
		pkt[i] = arp->arp_sha.octet[i];
		pkt[i+32] = arp->arp_sha.octet[i];
		pkt[i+6] = hinfo->haddr.octet[i];
		pkt[i+22] = hinfo->haddr.octet[i];
	}
	i = 12;
	pkt[i++] = (ETHERTYPE_ARP >> 8) & 0xff;
	pkt[i++] = ETHERTYPE_ARP & 0xff;
	pkt[i++] = (arp->arp_hrd >> 8) & 0xff;
	pkt[i++] = arp->arp_hrd & 0xff;
	pkt[i++] = (arp->arp_pro >> 8) & 0xff;
	pkt[i++] = arp->arp_pro & 0xff;
	pkt[i++] = arp->arp_hln;
	pkt[i++] = arp->arp_pln;
	pkt[i++] = 0;
	pkt[i] = ARPOP_REPLY;
	for (i = 0; i < 4; i++) {
		pkt[28+i] = (arp->arp_tpa >> (3-i)*8) & 0xff;
		pkt[38+i] = (arp->arp_spa >> (3-i)*8) & 0xff;
	}
	return pkt;
}

static void flood(const uint8_t *packet, int length)
{
	int i, j, swnum;
	struct entity **esw = topo_get_switches(&swnum);
	bool tports[MAX_PORT_NUM +1]; // TODO max_port_num, port num

	for (i = 0; i < swnum; i++){
		struct xswitch *xsw = entity_get_xswitch(esw[i]);
		int n_ports = xswitch_get_num_ports(xsw);
		int numadjs;
		const struct entity_adj *esw_adj = entity_get_adjs(esw[i], &numadjs);

		for (j = 1; j <= n_ports; j++)
			tports[j] = false;
		for (j = 0; j < numadjs; j++) {
			if(entity_get_type(esw_adj[j].adj_entity) == ENTITY_TYPE_SWITCH)
				tports[esw_adj[j].out_port] = true;
		}
		for (j = 1; j <= n_ports; j++) {
			if (tports[j] == false) {
				port_packet_out(xsw, j, packet, length);
			}
		}
	}
}

static int handle_arp_packet_in(const uint8_t *packet, int length, struct xswitch *xsw, int port)
{
	int i;
	struct entity *eh1;
	struct host_info hinfo;
	struct arp_header arp;

	if (length < 14 + 28)
		return -11;
	arp.arp_hrd = value_to_16(value_extract(packet, 14*8, 16));
	arp.arp_pro = value_to_16(value_extract(packet, 16*8, 16));
	arp.arp_hln = packet[18];
	arp.arp_pln = packet[19];
	arp.arp_op  = value_to_16(value_extract(packet, 20*8, 16));
	for (i = 0; i < 6; i++) {
		arp.arp_sha.octet[i] = packet[22+i];
		arp.arp_tha.octet[i] = packet[32+i];
	}
	arp.arp_spa = value_to_32(value_extract(packet, 28*8, 32));
	arp.arp_tpa = value_to_32(value_extract(packet, 38*8, 32));

	// handle arp request
	topo_rdlock();
	if (arp.arp_op == ARPOP_REQUEST) {
		eh1 = topo_get_host_by_paddr(arp.arp_tpa);
		if (eh1 != NULL) {
			//// found host entity
			// respond arp:
			// construct arp reply packet
			// packet_out
			int len;
			uint8_t *arp_reply_pkt;
			hinfo = entity_get_addr(eh1);
			arp_reply_pkt = arp_reply_packet_construct(&hinfo, &arp, &len);
			port_packet_out(xsw, port, arp_reply_pkt, len);
			free(arp_reply_pkt);
		} else {
			//// not found, waiting for reply.
			// save src host info.
			// flood arp request.
			struct entity *eh_wait;
			pthread_mutex_lock(&arp_lock);
			if (next_available_waiting_host >= 20) {
				fprintf(stderr, "waiting hosts num >= 20.\n");
				pthread_mutex_unlock(&arp_lock);
				topo_unlock();
				return -13;
			}
			eh_wait = topo_get_host_by_haddr(arp.arp_sha);
			arp_waiting_hosts[next_available_waiting_host] = eh_wait;
			get_next_available_waiting_host();
			pthread_mutex_unlock(&arp_lock);

			flood(packet, length);
		}
	} else if (arp.arp_op == ARPOP_REPLY) {
		// TODO respond waiting host and move it from waiting hosts.
		struct entity *ewait = NULL;
		struct entity *esw = NULL;
		struct xswitch *xsw = NULL;
		int sw_out_port;
		pthread_mutex_lock(&arp_lock);
		for (i = 0; i < 20; i++) {
			if(arp_waiting_hosts[i] != NULL) {
				hinfo = entity_get_addr(arp_waiting_hosts[i]);
				if (haddr_equal(hinfo.haddr, arp.arp_tha)){
					ewait = arp_waiting_hosts[i];
					arp_waiting_hosts[i] = NULL;
					if (i < next_available_waiting_host)
						next_available_waiting_host = i;
					break;
				}
			}
		}
		pthread_mutex_unlock(&arp_lock);

		if (ewait == NULL) {
			fprintf(stderr, "not find waiting arp host\n");
			topo_unlock();
			return -14;
		}
		esw = entity_host_get_adj_switch(ewait, &sw_out_port);
		xsw = entity_get_xswitch(esw);
		port_packet_out(xsw, sw_out_port, packet, length);
	}
	topo_unlock();
	return 0;
}

static void update_hosts(const uint8_t *packet, int len, struct xswitch *xsw, int port)
{
	struct entity *host;
	struct entity *esw;
	struct entity *old_esw;
	struct host_info hinfo;
	int old_port;
	uint16_t eth_type;
	uint32_t old_paddr;
	const struct entity_adj *esw_adj;
	int j, numadjs;

	hinfo.haddr = value_to_haddr(value_extract(packet, 48, 48));
	eth_type = value_to_16(value_extract(packet, 96, 16));
	switch (eth_type) {
	case ETHERTYPE_ARP:
		assert(len >= 14 + 28);
		hinfo.paddr = value_to_32(value_extract(packet, 28*8, 32));
		break;
	case ETHERTYPE_IP:
		assert(len >= 14 + 20);
		hinfo.paddr = value_to_32(value_extract(packet, 26*8, 32));
		break;
	default:
		hinfo.paddr = 0;
		break;
	}

	/* fast path */
	topo_rdlock();
	esw = topo_get_switch(xswitch_get_dpid(xsw));
	assert(esw != NULL);

	/* Make sure the packet is sent by a host directly. */
	esw_adj = entity_get_adjs(esw, &numadjs);
	for (j = 0; j < numadjs; j++) {
		if(esw_adj[j].out_port == port &&
		   entity_get_type(esw_adj[j].adj_entity) == ENTITY_TYPE_SWITCH) {
			topo_unlock();
			return;
		}
	}

	/* is multicast addr? */
	if((hinfo.haddr.octet[0] & 1) == 0) {
		host = topo_get_host_by_haddr(hinfo.haddr);
		if(host) {
			old_esw = entity_host_get_adj_switch(host, &old_port);
			old_paddr = entity_get_addr(host).paddr;
			if (old_esw == esw && old_port == port &&
			    ((hinfo.paddr && old_paddr == hinfo.paddr) ||
			     !hinfo.paddr)) {
				topo_unlock();
				return;
			}
		}
	}
	topo_unlock();

	/* slow path */
	topo_wrlock();
	host = topo_get_host_by_haddr(hinfo.haddr);
	if (host == NULL) {
		host = entity_host(hinfo);
		if (topo_add_host(host) >= 0)
			entity_add_link(host, 1, esw, port);
		fprintf(stderr, "found new host:\n");
		entity_print(host);
	} else {
		old_esw = entity_host_get_adj_switch(host, &old_port);
		old_paddr = entity_get_addr(host).paddr;
		if (hinfo.paddr && old_paddr != hinfo.paddr) {
			if(old_paddr) {
				fprintf(stderr, "IP alias detected!\n");
				fprintf(stderr, "mac: "
					"%02x:%02x:%02x:%02x:%02x:%02x\n",
					hinfo.haddr.octet[0],
					hinfo.haddr.octet[1],
					hinfo.haddr.octet[2],
					hinfo.haddr.octet[3],
					hinfo.haddr.octet[4],
					hinfo.haddr.octet[5]);
				fprintf(stderr, "old ip: %08x\n", old_paddr);
				fprintf(stderr, "new ip: %08x\n", hinfo.paddr);
				assert(0);
			} else {
				entity_set_paddr(host, hinfo.paddr);
				fprintf(stderr, "host paddr changed:\n");
				entity_print(host);
			}
		}
		if (old_esw != esw || old_port != port) {
			topo_del_host(host);
			host = entity_host(hinfo);
			if (topo_add_host(host) >= 0)
				entity_add_link(host, 1, esw, port);
			fprintf(stderr, "host loc changed:\n"
				"old: (%08x, %d)\nnew: (%08x, %d)\n",
				entity_get_dpid(old_esw),
				old_port,
				entity_get_dpid(esw),
				port);
		}
	}
	topo_print();
	topo_unlock();
}

int
handle_topo_packet_in(struct xswitch *sw, int port, const uint8_t *packet, int length)
{
	if (length < 14)
		return -1;

	uint16_t eth_type = value_to_16(value_extract(packet, 96, 16));
	if (eth_type == LLDP_TYPE) {
		handle_lldp_packet_in(packet, length, sw, port);
		topo_rdlock();
		topo_print();
		topo_unlock();
		return -3;
	} else if (eth_type == ETHERTYPE_ARP) {
		update_hosts(packet, length, sw, port);
		handle_arp_packet_in(packet, length, sw, port);
		return -4;
	} else {
		update_hosts(packet, length, sw, port);
		return -2;
	}
}
