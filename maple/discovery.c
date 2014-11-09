#include "discovery.h"
// install flow table
// construct LLDP packet
// parse LLDP packet
// send LLDP packet

// static route_t links = {0};
static struct entity *arp_waiting_hosts[20];
static int next_available_waiting_host;

static void inline get_next_available_waiting_host()
{
	int i;
	for (i = 0; i < 20; i++)
		if (arp_waiting_hosts[i] == NULL)
			break;
	next_available_waiting_host = i;
}
int 
arp_default_flow_install(struct xswitch *sw, int prio)
{
        struct match *ma;
        struct msgbuf *msg;
        struct action *ac;
        int idx;
        ma = match();
        idx = flow_table_get_field_index(sw->table0, "dl_type");
        if(idx >= 0)
                match_add(ma, idx, value_from_16(ETHERTYPE_ARP), value_from_16(0xffff));
        ac = action();
        action_add(ac, AC_PACKET_IN, 0); 
        msg = msg_flow_entry_add(sw->table0, prio, ma, ac);
        match_free(ma);
        action_free(ac);
        xswitch_send(sw, msg);
        return prio;
}

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

static uint8_t * 
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
/*
void 
entities_update_old_link(edge_t *link)
{
	// TODO realize modify topo;
	dpid_t dpid1 = link->dpid1;
	dpid_t dpid2 = link->dpid2;
	uint16_t port1 = link->port1;
	struct entity *e2 = topo_get_switch(dpid2);
	entity_add_link(e1, port1, e2, port2);
}
static void
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
*/
// return length of the tlv
static uint16_t
lldp_tlv_next(const uint8_t *packet, struct lldp_tlv *tlv/*OUT*/, uint16_t offset)
{
	uint16_t v = value_to_16(value_extract(packet, offset, 16));
        tlv->type = v >> 9;
        tlv->length = v & 511;
        assert((tlv->type < 9 && tlv->type >=0) || tlv->type == 127);
        assert(tlv->length >= 0 && tlv->length < 512);
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
		printf("type: %d\n", type);
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
	link->dpid1 = dpid;
	link->port1 = port;
	return true;
}
// struct packet_in;
int
handle_lldp_packet_in(const struct packet_in *packet_in)
{
	//if (links.total_size == 0)
	//	route_init(&links);
	const uint8_t *packet = packet_in_get_packet(packet_in);
	uint16_t length = packet_in_get_length(packet_in);
	if (length < 14 + 14)
	{
		printf("length < 28\n");
		return -1;
	}
	port_t port = packet_in_get_port(packet_in);
	dpid_t dpid = packet_in_get_dpid(packet_in);
	uint16_t eth_type = value_to_16(value_extract(packet, 96, 16));
	if (eth_type != LLDP_TYPE)
		return -2;
	edge_t link;
	link.dpid2 = dpid;
	link.port2 = port;
	if (! parse_lldp(packet, &link))
	{
		printf("parse_lldp error\n");
		return -3;
	}
	// route_add_edge(&links, &link);
	if (! edge_valid(&link))
	{
		printf("edge_valid\n");
		return -4;
	}
	struct entity *e1 = topo_get_switch(link.dpid1);
	struct entity *e2 = topo_get_switch(link.dpid2);
	entity_add_link(e1, link.port1, e2, link.port2);
#if 0
	uint8_t retval = route_update_edge(&links, &link);
	switch (retval) {
		case 0:
			break;
		case 1:
			// entities_update_old_link(&link);
			entities_add_new_link(&links, &link);
			break;
		case 2:
			entities_add_new_link(&links, &link);
			break;
		default:;
	}
	printf("----------\n\n");
	route_print(&links);
#endif
	return 0;
}
struct etherhdr {
	uint8_t ether_dhost[6];
	uint8_t ether_shost[6];
	uint16_t ether_type;
};
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

	struct haddr arp_sha;
	uint32_t arp_spa;
	struct haddr arp_tha;
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
struct flood_node {
	struct entity *esw;
	int out_num;
	uint8_t in_port;
	port_t out_port[16];
};
#if 0
static void flood_node_add_port(struct flood_node *fnode, port_t port)
{
	assert(fnode->out_num < 16);
	fnode->out_port[fnode->out_num] = port;
	fnode->out_num ++;
}
static void arp_flow_install(struct flood_node *fnode, value_t srchaddr, int prio)
{
	struct match *ma;
	struct msgbuf *msg;
	struct action *ac;
	struct xswitch *xsw;
	int index, i;
	xsw = entity_get_xswitch(fnode->esw);
	ma = match();
	index = flow_table_get_field_index(xsw->table0, "in_port");
	if (index >= 0)
		match_add(ma, index, value_from_8(fnode->in_port), value_from_8(0xff));
	index = flow_table_get_field_index(xsw->table0, "dl_src");
	if (index >= 0)
		match_add(ma, index, srchaddr, value_from_48(0xffffffffffff));
	index = flow_table_get_field_index(xsw->table0, "dl_type");
	if (index >= 0)
		match_add(ma, index, value_from_16(ETHERTYPE_ARP), value_from_16(0xffff));
	ac = action();
	for (i = 0; i < fnode->out_num; i++)
		action_add(ac, AC_OUTPUT, fnode->out_port[i]);
	msg = msg_flow_entry_add(xsw->table0, prio, ma, ac);
	match_free(ma);
	action_free(ac);
	xswitch_send(xsw, msg);
}
static void arp_request_packet_out(struct flood_node *fnode, const uint8_t *pkt, uint16_t len)
{
	struct action *out_ac;
	struct msgbuf *mb;
	int i;
	struct xswitch *xsw;
	xsw = entity_get_xswitch(fnode->esw);
	out_ac = action();
	for (i=0; i < fnode->out_num; i++)
		action_add(out_ac, AC_OUTPUT, fnode->out_port[i]);
	mb = msg_packet_out(0, pkt, len, out_ac);
	action_free(out_ac);
	xswitch_send(xsw, mb);
}
static void flood2(const uint8_t *packet, uint16_t length, dpid_t dpid, port_t port)
{
	struct nodeinfo *nodes;
	struct entity **esws;
	struct entity *srcsw;
	int eswnum;
	int i, j;
	struct flood_node fnode;
	value_t srchaddr;
	for (i = 0; i < 6; i++)
		srchaddr.v[i] = packet[i+6];
	esws = topo_get_switches(&eswnum);
	srcsw = topo_get_switch(dpid);
	nodes = spanning_tree_init(srcsw, port, esws, eswnum);
	for (i=0; i<eswnum; i++) {
		fnode.esw = esws[i];
		// XXX match in_port: 8 bit.
		fnode.in_port = (uint8_t)nodes[i].in_port;
		fnode.out_num = 0;
		for (j=i; j<eswnum; j++) {
			if (nodes[j].parent == i) {
				flood_node_add_port(&fnode, nodes[j].parent_out_port);
			}
		}
		// install arp flow table
		arp_flow_install(&fnode, srchaddr, 2);
		// packet out
		arp_request_packet_out(&fnode, packet, length);
	}
}
#endif
static void port_packet_out(struct xswitch *xsw, port_t port, const uint8_t *pkt, uint16_t len)
{
	struct action *ac;
	struct msgbuf *mb;
	ac = action();
	action_add(ac, AC_OUTPUT, port);
	mb = msg_packet_out(0, pkt, len, ac);
	action_free(ac);
	xswitch_send(xsw, mb);
}
static void flood(const uint8_t *packet, uint16_t length, dpid_t dpid, port_t port)
{
	int i, j, swnum, numadjs;
	struct entity **esw = topo_get_switches(&swnum);
	struct xswitch *xsw;
	const struct entity_adj *esw_adj;
	uint8_t tports[MAX_PORT_NUM +1] = {0};// TODO max_port_num, port num
	for (i = 0; i < MAX_ENTITY_NUM; i++){
		if (esw[i] == NULL)
			continue;
		xsw = entity_get_xswitch(esw[i]);
		esw_adj = entity_get_adjs(esw[i], &numadjs);
		for (j = 0; j <= xsw->n_ports; j++)
			tports[j] = 0;
		for (j = 0; j < numadjs; j++) {
			// printf("out_port: %d\n", esw_adj[j].out_port);
			tports[esw_adj[j].out_port] = 1;
		}
		for (j = 1; j <= xsw->n_ports; j++) {
			if (tports[j] == 0) {
				port_packet_out(xsw, j, packet, length);
			}
		}
	}
}
int handle_arp_packet_in(const uint8_t *packet, uint16_t length, dpid_t dpid, port_t port)
{
	int i;
	struct entity *eh1;
	struct host_info hinfo;
	// struct etherhdr ehhdr;
	struct arp_header arp;
	struct entity *esw;
	struct xswitch *xsw;

	
	if (length < 14 + 28)
		return -11;
	// parse arp
	/*
	for (i=0; i<6; i++){
		ehhdr.ether_dhost[i] = packet[i];
		ehhdr.ether_shost[i+6] = packet[i+6];
	}
	ehhdr.ether_type = ETHERTYPE_ARP;*/
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
	if (arp.arp_op == ARPOP_REQUEST) {
		eh1 = topo_get_host_by_paddr(arp.arp_tpa);
		if (eh1 != NULL) {
			//// found host entity
			// respond arp:
			// construct arp reply packet
			// packet_out
			int len;
			uint8_t *arp_reply_pkt;
			struct action *out_a;
			struct msgbuf *mb;
			///// printf("----------send packet--%d, %d\n", dpid, port);

			hinfo = entity_get_addr(eh1);
			arp_reply_pkt = arp_reply_packet_construct(&hinfo, &arp, &len);
			//// send reply packet back;
			out_a = action();
			action_add(out_a, AC_OUTPUT, port);
			mb = msg_packet_out(0, arp_reply_pkt, len, out_a);
			action_free(out_a);
			free(arp_reply_pkt);
			esw = topo_get_switch(dpid);
			assert(esw != NULL);
			xsw = entity_get_xswitch(esw);
			xswitch_send(xsw, mb);
		} else {
			
			//// not found, waiting for reply.
			// save src host info.
			// flood arp request.
			struct entity *eh_wait;
			if (next_available_waiting_host >= 20) {
				printf("waiting hosts num >= 20.\n");
				return -13;
			}
			eh_wait = topo_get_host_by_haddr(arp.arp_sha);
			arp_waiting_hosts[next_available_waiting_host] = eh_wait;

			// flood :
			// generate mulitcast tree
			// set matcH
			// install flow table
			// packet_out
			flood(packet, length, dpid, port);
			get_next_available_waiting_host();
		
		}
	} else if (arp.arp_op == ARPOP_REPLY) {
		// TODO respond waiting host and move it from waiting hosts.
		struct entity *ewait = NULL;
		const struct entity_adj *ehadj = NULL;
		struct entity *esw = NULL;
		struct xswitch *xsw = NULL;
		port_t sw_out_port;
		int adjnum;
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
		if (ewait == NULL) {
			printf("not find waiting arp host\n");
			return -14;
		}
		ehadj = entity_get_adjs(ewait, &adjnum);
		assert(adjnum == 1);
		esw = ehadj[0].adj_entity;
		sw_out_port = ehadj[0].adj_in_port;
		xsw = entity_get_xswitch(esw);
		port_packet_out(xsw, sw_out_port, packet, length);
	}
	return 0;
}
static void update_hosts(const uint8_t *packet, uint16_t len, dpid_t dpid, port_t port)
{
	struct entity *host, *phost;
	struct entity *esw;
	struct entity *oldesw;
	struct haddr hsrc_addr;
	struct host_info hinfo;
	uint32_t psrc_addr = 0;
	port_t sw_port;
	int i;
	uint16_t eth_type = value_to_16(value_extract(packet, 96, 16));
	for (i = 0; i < 6; i++)
		hsrc_addr.octet[i] = packet[i+6];
	switch (eth_type) {
		case ETHERTYPE_ARP:
			assert(len >= 14 + 28);
			psrc_addr = value_to_32(value_extract(packet, 28*8, 32));
			break;
		case ETHERTYPE_IP:
			assert(len >= 14 + 20);
			psrc_addr = value_to_32(value_extract(packet, 26*8, 32));
			break;
		default:
			// return;
			break;
	}
	hinfo.haddr = hsrc_addr;
	hinfo.paddr = psrc_addr;
	esw = topo_get_switch(dpid);
	assert(esw != NULL);
	host = topo_get_host_by_haddr(hsrc_addr);
	phost = topo_get_host_by_paddr(psrc_addr);
	if (host != phost) {
		// delete the hhost and the phsot, add a new host
		if (host != NULL) {
			oldesw = entity_host_get_adj_switch(host, &sw_port);
			topo_del_host(host);
			entity_adj_down(oldesw, sw_port);
		}
		if (phost != NULL) {
			oldesw = entity_host_get_adj_switch(phost, &sw_port);
			topo_del_host(phost);
			entity_adj_down(oldesw, sw_port);
		}
		host = entity_host(hinfo);
		if (topo_add_host(host) >= 0)
			entity_add_link(host, 1, esw, port);
	} else if (host == NULL) {
		host = entity_host(hinfo);
		if (topo_add_host(host) >= 0)
			entity_add_link(host, 1, esw, port);
	} else {
		oldesw = entity_host_get_adj_switch(host, &sw_port);
		if (oldesw == esw)
			return;
		entity_adj_down(oldesw, sw_port);
		topo_del_host(host);
		host = entity_host(hinfo);
		if (topo_add_host(host) >= 0)
			entity_add_link(host, 1, esw, port);
	}
	/*
	int hnum;
	struct entity **hs = topo_get_hosts(&hnum);
	printf("hnum. %d\n", hnum);
	for (i = 0 ;i < hnum; i++) {
		if (hs[i] != NULL)
			printf("not nulllllll\n");
	}*/
}

int handle_topo_packet_in(const struct packet_in *packet_in)
{
	const uint8_t *packet = packet_in_get_packet(packet_in);
	uint16_t length = packet_in_get_length(packet_in);
	port_t port = packet_in_get_port(packet_in);
	dpid_t dpid = packet_in_get_dpid(packet_in);
	if (length < 14)
	{
		// printf("length < 14\n");
		return -1;
	}
	uint16_t eth_type = value_to_16(value_extract(packet, 96, 16));
	printf("eth type:%x\n", eth_type);
	if (eth_type == LLDP_TYPE) {
		handle_lldp_packet_in(packet_in);
		topo_print();
		return -3;
	} else if (eth_type == ETHERTYPE_ARP) {
		update_hosts(packet, length, dpid, port);
		handle_arp_packet_in(packet, length, dpid, port);
		topo_print();
		return -4;
	} else {
		// TODO
		// update_hosts(packet, length, dpid, port);
		// topo_print();
		return -2;
	}
}
