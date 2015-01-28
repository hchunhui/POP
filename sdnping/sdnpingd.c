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

uint8_t macaddr[6];
uint32_t local_dpid;
uint16_t local_port;

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

int
get_sdn_addr()
{
	FILE *fp;
	fp = fopen("sdnpos.conf", "r");
	if (!fp) {
		perror("get_sdn_addr(): fopen(): sdnpos.conf");
		return -1;
	}
	if (fscanf(fp, "%d\n%hd", &local_dpid, &local_port) != 2) {
		printf("fscanf(): error, cannot read two local_dpid or local_port, please check sdnpos.conf\n");
		return -1;
	}
	fclose(fp);
	// printf("local_dpid:%d\nlocal_port:%hd\n", local_dpid, local_port);
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
		printf("get_mac_addr(): %s\n", name);
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
construct_reply_packet(uint32_t remote_dpid, uint16_t remote_port,
		       uint16_t sequence_num, uint8_t *pkt)
{
	int i = 0;
	uint32_t t32;
	uint16_t t16;

	memset(pkt, 0, PKT_LEN);
	memset(pkt, 0xff, 6);
	i += 6;
	memcpy(&pkt[i], macaddr, 6);
	i += 6;
	pkt[i++] = 0x55;
	pkt[i++] = 0x55;
	pkt[i] = 0x2;
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

	return 0;
}
void
print_packet(const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
	int i;

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
	pcap_t *device = (pcap_t *)args;
	int i;
	struct sphdr sph;
	uint8_t sendbuf[PKT_LEN];
	i = 14;
	sph.type = packet[i++];
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
		// printf("recv request from switch %d port %d\n",
		//         sph.src_dpid, sph.src_port);
		construct_reply_packet(sph.src_dpid, sph.src_port, sph.sequence_num , sendbuf);
		pcap_inject(device, sendbuf, PKT_LEN);
	}

	// print_packet(pkthdr, packet);

}

int
main()
{
	char err_buf[PCAP_ERRBUF_SIZE], *devstr;
	struct bpf_program filter;
	pcap_t *device;

	if (fork() != 0)
		return 0;
	devstr = pcap_lookupdev(err_buf);
	// devstr = "h1-eth1";
	printf("devstr: %s\n", devstr);
	if (!devstr) {
		printf("error: %s\n", err_buf);
		exit(1);
	}

	if (get_mac_addr(devstr) < 0) {
		return 1;
	}
	if (get_sdn_addr() < 0) {
		return 1;
	}
	device = pcap_open_live(devstr, 65535, 1, 0, err_buf);
	if (!device) {
		printf("error: pcap_open_live(): %s\n", err_buf);
		return 1;
	}

	pcap_setdirection(device, PCAP_D_IN);
	// construct a filter
	pcap_compile(device, &filter, "ether[12]=0x55 and ether[13]=0x55 and ether[14]=0x1", 1, 0);
	pcap_setfilter(device, &filter);

	pcap_loop(device, -1, handle_packet, (u_char *)device);

	pcap_close(device);

	return 0;
}
