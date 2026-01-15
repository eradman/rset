/* miniquark: see LICENSE file for copyright and license details. */

#include <sys/socket.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "sock.h"

int
addr_listen(const char *host, const char *port, struct pollfd *pfd, struct sockaddr_storage *sa) {
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *ai, *p;
	int ret, insock = 0;
	int addr_count = 0;

	ret = getaddrinfo(host, port, &hints, &ai);
	if (ret != 0)
		err(1, "getaddrinfo: %s", gai_strerror(ret));

	for (p = ai; p && (addr_count < LISTEN_MAX); p = p->ai_next) {
		if ((insock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			err(1, "socket");
		if (setsockopt(insock, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0)
			err(1, "setsockopt");
		if (bind(insock, p->ai_addr, p->ai_addrlen) < 0)
			err(1, "bind");
		if (listen(insock, SOMAXCONN) < 0)
			err(1, "listen");

		pfd[addr_count].fd = insock;
		pfd[addr_count].events = POLLIN;
		memcpy(&sa[addr_count], p->ai_addr, sizeof(struct sockaddr_storage));
		addr_count++;
	}

	freeaddrinfo(ai);
	return addr_count;
}

int
sock_set_timeout(int fd, int sec) {
	struct timeval tv;

	tv.tv_sec = sec;
	tv.tv_usec = 0;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0
	    || setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		warn("setsockopt");
		return 1;
	}

	return 0;
}

int
inaddr_to_str(const struct sockaddr_storage *in_sa, char *str, size_t len) {
	switch (in_sa->ss_family) {
	case AF_INET:
		if (!inet_ntop(AF_INET, &((struct sockaddr_in *) in_sa)->sin_addr, str, len)) {
			warn("inet_ntop");
			return 1;
		}
		break;
	case AF_INET6:
		if (!inet_ntop(AF_INET6, &((struct sockaddr_in6 *) in_sa)->sin6_addr, str, len)) {
			warn("inet_ntop");
			return 1;
		}
		break;
	}

	return 0;
}
