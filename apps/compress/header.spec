header tcpip {
	fields {
		__dl_dst : 48;
		__dl_src : 48;
		dl_type : 16;
		__ver : 4;
		__ihl : 4;
		__tos : 8;
		__len : 16;
		__id : 16;
		__flag : 3;
		__off : 13;
		__ttl : 8;
		nw_proto : 8;
		__sum : 16;
		nw_src : 32;
		nw_dst : 32;
		tp_src : 16;
		tp_dst : 16;
	}
}

start tcpip;
