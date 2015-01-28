#include <pcap.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>

#define PKT_LEN 60
// HWaddr 78:2B:CB:11:1B:DA
// TODO type
//
uint8_t macaddr[6];
uint32_t local_dpid;
uint16_t local_port;
uint16_t sequence_num;
int id;

struct sphdr {
	uint8_t type;
	uint8_t reason;
	uint16_t checksum;
	uint32_t src_dpid;
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t dst_dpid;
	uint16_t sequence_num;
};

void
print_packet(const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
	int i;

	printf("id: %3d\n", ++id);
	printf("Packet length: %4d\n", pkthdr->len);
	printf("Number of bytes: %4d\n", pkthdr->caplen);
	printf("Recived time: %s\n", ctime((const time_t *)&pkthdr->ts.tv_sec));

	for (i = 0; i < pkthdr->len; i++) {
		printf(" %02x", packet[i]);
		if ((i+1) % 16 == 0)
			printf("\n");
	}
	printf("\n\n");
}

void
handle_packet(u_char *args, const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
	// printf ("handle packet %d\n", sequence_num);
	int *ret = (int *)args;
	int i;
	struct sphdr sph;
	i = 14;
	sph.type = packet[i++];
	if (sph.type != 2) {
		*ret = -1;
		return;
	}
	sph.reason = packet[i++];
	sph.checksum = ntohs(*((uint16_t *)&packet[i]));
	i += 2;
	sph.src_dpid = ntohl(*((uint32_t *)&packet[i]));
	i += 4;
	sph.src_port = ntohs(*((uint16_t *)&packet[i]));
	i += 2;
	sph.dst_port = ntohs(*((uint16_t *)&packet[i]));
	i += 2;
	sph.dst_dpid = ntohl(*((uint32_t *)&packet[i]));
	i += 4;
	sph.sequence_num = ntohs(*((uint16_t *)&packet[i]));

	if (sph.dst_port == local_port && sph.dst_dpid == local_dpid) {
		if (sph.reason == 0) {
			*ret = 1;
			printf("recv ack %3d from switch: %4d port: %4d\n",
				sph.sequence_num, sph.src_dpid, sph.src_port);
		} else if (sph.reason == 1) {
			*ret = -2;
		}
	} else {
		*ret = -1;
		return;
	}

	// print_packet(pkthdr, packet);

}


int
get_sdn_addr()
{
	FILE *fp;
	fp = fopen("sdnsend.conf", "r");
	if (!fp) {
		perror("get_sdn_addr(): fopen(): sdnsend.conf");
		return -1;
	}
	if (fscanf(fp, "%d\n%hd", &local_dpid, &local_port) != 2) {
		printf("fscanf(): error, cannot read two local_dpid ");
		printf("or local_port, please check sdnpos.conf\n");
		return -1;
	}
	fclose(fp);
	printf("local_dpid:%d\nlocal_port:%hd\n", local_dpid, local_port);
	return 0;
}
int
get_mac_addr(char *name)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1) {
		perror("get_mac_addr(): socket():");
		return -1;
	}
	// ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, name, IFNAMSIZ-1);

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
		perror("get_mac_addr(): ioctl():");
		close(fd);
		return -1;
	}
	close(fd);
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		printf("get_mac_addr(): not an Ethernet interface\n");
		return -1;
	}
	memcpy(macaddr, ifr.ifr_hwaddr.sa_data, 6);
	return 0;
}

int
construct_request_packet(uint32_t remote_dpid, uint16_t remote_port, uint8_t *pkt)
{
	int i = 0;
	uint32_t t32;
	uint16_t t16;
	memset(pkt, 0, PKT_LEN);

	// set mac header
	memset(pkt, 255, 6);
	i += 6;
	memcpy(&pkt[i], macaddr, 6);
	i += 6;
	pkt[i++] = 0x55;
	pkt[i++] = 0x55;

	// set sdn new protocol header
	pkt[i] = 0x1;
	i += 4;
	t32 = htonl(local_dpid);
	t16 = htons(local_port);
	memcpy(&pkt[i], &t32, 4);
	i += 4;
	memcpy(&pkt[i], &t16, 2);
	i += 2;
	t32 = htonl(remote_dpid);
	t16 = htons(remote_port);
	memcpy(&pkt[i], &t16, 2);
	i += 2;
	memcpy(&pkt[i], &t32, 4);
	i += 4;
	t16 = htons(sequence_num);
	memcpy(&pkt[i], &t16, 2);

	// TODO Sequence Number and CheckSum
	return 0;
}

int
main(int argc, char *argv[])
{
	// lookup and open dev
	char errbuf[PCAP_ERRBUF_SIZE], *devstr;
	pcap_t *device;
	uint32_t remote_dpid;
	uint16_t remote_port;
	uint8_t sendbuf[PKT_LEN];
	struct bpf_program filter;
	int t;

	if (argc < 3) {
		printf("Usage: sdnping dst_dpid dst_port\n");
		printf("type of dst_dpid, dst_port: int\n");
		return 1;
	}
	t = atoi(argv[1]);
	if (t <= 0) {
		printf("Range of dst_dpid is >= 1\n");
		return 1;
	}
	remote_dpid = (uint32_t) t;
	t = atoi(argv[2]);
	if (t <= 0) {
		printf("Ragne of dst_port is >= 1\n");
		return 1;
	}
	remote_port = (uint16_t) t;
	// devstr = "h2-eth1";
	devstr = pcap_lookupdev(errbuf);
	if (devstr) {
		printf("success: devstr: %s\n", devstr);
	} else {
		printf("error: %s\n", errbuf);
		return 1;
	}

	if (get_mac_addr(devstr) < 0) {
		return 1;
	}
	if (get_sdn_addr() < 0) {
		return 1;
	}

	device = pcap_open_live(devstr, 65535, 1, 0, errbuf);
	if (!device) {
		printf("error: pcap_open_live(): %s\n", errbuf);
		return 1;
	}


	pcap_setdirection(device, PCAP_D_IN);
	pcap_compile(device, &filter, "ether[12]=0x55 and ether[13]=0x55", 1, 0);
	pcap_setfilter(device, &filter);

	// construct request packet

	for(;;) {
		int ret;
		sequence_num ++;
		printf("send packet %3d\n", sequence_num);
		construct_request_packet(remote_dpid, remote_port, sendbuf);
		pcap_inject(device, sendbuf, PKT_LEN);
		for(;;) {
			// XXX set id as a return value
			// USE dispatch because loop has no time out
			// if true, break
			// else goto dispatch
loop:
			ret = 0;
			pcap_dispatch(device, 1, handle_packet, (u_char *)&ret);
			if (ret == 0) {
				printf("time out\n");
				break;
			} else if (ret == 1) {
				// cal and print used time;
				break;
			} else if (ret == -1) {
				// not reply pkt
				printf("recv a non reply packet\n");
				goto loop;
			} else if (ret == -2) {
				printf("the port doesnot exist\n");
				goto loop;
			}
			
			// XXX set id as return value
			// add time self
			// pcap_loop(device, -1, get_packet, (u_char *)&id);
		}
		sleep(1);
	}

	pcap_close(device);

	return 0;
}
