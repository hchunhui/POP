
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include "msgqueue.h"
#include "msgbuf.h"
#include "pof/pof_global.h"

/*
 * Return value:
 *  0 : closed connection
 *  1 : success
 * -1 : failure
 */
int
recv_packet(int fd, struct msgbuf *rx)
{
	int retval;
	int want_bytes;
	int size = 0;

#ifdef DEBUG_NONBLOCK
	int count = 0;
#endif
again:

#ifdef DEBUG_NONBLOCK
	count++;
#endif
	if (sizeof(struct pof_header) > size) {
		/* 先把pof的包头recv过来 */
		want_bytes = sizeof(struct pof_header) - size;
	} else {
		struct pof_header *oh = (void *)rx->data;
		int length = ntohs(oh->length);
		if (length < sizeof(struct pof_header)) {
			fprintf(stderr, "received too-short pof_header (%d bytes)\n",
				length);
			errno = EPROTO;
#ifdef DEBUG_NONBLOCK
			{
				int i;
				printf("count = %d\n", count);
				printf("retval: %d\n", retval);
				printf("sizeof(struct pof_header): %lu\n",
					sizeof(struct pof_header));
				printf("size: %d\n", size);
				printf("version: %d\n", oh->version);
				printf("type: %d\n", oh->type);
				printf("length: %d\n", ntohs(oh->length));
				printf("xid: %d\n", ntohl(oh->xid));
				printf("ret: %d\n", recv(fd, rx->data, 8, MSG_DONTWAIT));
				for (i = 0; i < 8; i++)
					printf("%02x ", rx->data[i]);
				printf("\n");
				struct pof_header *oh = (void *)rx->data;
				printf("version: %d\n", oh->version);
				printf("type: %d\n", oh->type);
				printf("length: %d\n", ntohs(oh->length));
				printf("xid: %d\n", ntohl(oh->xid));
				for (i = 0; i < 8; i++)
					printf("%02x ", rx->data[i]);
				printf("\n");
				exit(1);
			}
#endif
			return (-1);
		}
		want_bytes = length - size;

		if (!want_bytes)
			return (1);
	}

	retval = read(fd, rx->data + size, want_bytes);
	//retval = recv(fd, rx->data + size, want_bytes, MSG_DONTWAIT);
	if (retval < 0) {
		if (errno != EAGAIN)
			fprintf(stderr, "failed to recv from remote: %s\n",
				strerror(errno));
		return (-1);
	}
	if (retval == 0)
		return (0); /* closed connection */

	size += retval;
	goto again;
}
