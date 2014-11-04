header ipv4;
header arp;
header icmp;
header igmp;
header tcp;
header udp;

header ethernet {
	fields {
		dl_dst : 48;
		dl_src : 48;
		dl_type : 16;
	}
	next select (dl_type) {
		case 0x0800: ipv4;
		case 0x0806: arp;
	}
}

header ipv4 {
	fields {
		ver : 4;
		ihl : 4;
		tos : 8;
		len : 16;
		id : 16;
		flag : 3;
		off : 13;
		ttl : 8;
		nw_proto : 8;
		sum : 16;
		nw_src : 32;
		nw_dst : 32;
		opt : *;
	}
	//length : ihl * 4;
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
	}
}

header udp {
	fields {
		tp_src : 16;
		tp_dst : 16;
	}
}

start ethernet;
