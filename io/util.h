/*
 * Author: Tiwei Bie (btw@mail.ustc.edu.cn)
 */

#ifndef _IO_UTIL_H
#define _IO_UTIL_H

#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "msgqueue.h"
#include "msgbuf.h"

int recv_packet(int fd, struct msgbuf *rx);

static inline int
adjust_bufsize(int fd)
{
	int ret;
        int optval;

	//int optlen;
	//ret = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);  
	//printf("optval: %d\n", optval);

	/* sysctl kern.ipc.maxsockbuf: 2097152 */
        //optval = 1024 * 2;  
        //optval = 1 * 1024 * 1024;
	optval = 33304 * 50;
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	if (ret == -1) {
		perror("setsockopt(SO_SNDBUF)");
		return (ret);
	}

	return (ret);
}

static inline int
make_fd_block(int fd)
{
	int ret = 0;

	ret = fcntl(fd, F_GETFL, 0);
	if (ret == -1) {
		perror("fcntl(F_GETFL)");
		return (ret);
	}

	ret = fcntl(fd, F_SETFL, ret & (~O_NONBLOCK));
	if (ret == -1) {
		perror("fcntl(F_SETFL)");
		return (ret);
	}

	return (ret);
}

static inline int
make_fd_nonblock(int fd)
{
	int ret = 0;

	ret = fcntl(fd, F_GETFL, 0);
	if (ret == -1) {
		perror("fcntl(F_GETFL)");
		return (ret);
	}

	ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
	if (ret == -1) {
		perror("fcntl(F_SETFL)");
		return (ret);
	}

	return (ret);
}

#endif
