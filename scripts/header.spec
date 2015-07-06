header ipv4;
header sdnp;
header ipv6;
header arp;
header icmp;
header igmp;
header tcp;
header udp;
header sdnaddr;

header ethernet {
	fields {
		dl_dst : 48;
		dl_src : 48;
		dl_type : 16;
	}
	next select (dl_type) {
		case 0x5555: sdnp;
		case 0x0800: ipv4;
		case 0x0806: arp;
		case 0x86dd: ipv6;
	}
}

header sdnp {
	/*
	fields {
		type : 8;
		__reason : 8;
		__len : 16;
		dpid_src : 32;
		port_src : 16;
		port_dst : 16;
		dpid_dst : 32;
		__seq_num : 16;
		cksum : 16;
	} */
	fields {
		len : 8;
		addr : 8;
		msg : *;
	}
	length : len;
	next select (addr) {
		case 0: sdnaddr;
	}
}

header sdnaddr {
	fields {
		type : 8;
		__reason : 8;
		dpid_src : 32;
		port_src : 16;
		port_dst : 16;
		dpid_dst : 32;
		__seq_num : 16;
	}
}

header ipv4 {
	fields {
		ver : 4;
		ihl : 4;
		__tos : 8;
		__len : 16;
		__id : 16;
		__flag : 3;
		__off : 13;
		ttl : 8;
		nw_proto : 8;
		sum : 16;
		nw_src : 32;
		nw_dst : 32;
		opt : *;
	}
	length : ihl << 2;
	checksum : sum;
	next select (nw_proto) {
		case 0x01 : icmp;
		case 0x02 : igmp;
		case 0x06 : tcp;
		case 0x11 : udp;
	}
}

header icmp {
	fields {
		type : 8;
		code : 8;
	}
}

header tcp {
	fields {
		tp_src : 16;
		tp_dst : 16;
		__seq : 32;
		__ack : 32;
		off : 4;
		__flags : 12;
		__win : 16;
		sum : 16;
		__urp : 16;
		opt : *;
	}
	length : off << 2;
}

header udp {
	fields {
		tp_src : 16;
		tp_dst : 16;
		len : 16;
		sum : 16;
	}
}

start ethernet;
