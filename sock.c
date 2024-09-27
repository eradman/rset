/* miniquark: see LICENSE file for copyright and license details. */

#include <sys/socket.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <unistd.h>

#include "sock.h"

int
sock_get_ips(const char *host, const char *port) {
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *ai, *p;
	int ret, insock = 0;

	if ((ret = getaddrinfo(host, port, &hints, &ai))) {
		err(1, "getaddrinfo: %s", gai_strerror(ret));
	}

	for (p = ai; p; p = p->ai_next) {
		if ((insock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			continue;
		}
		if (setsockopt(insock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
			err(1, "setsockopt");
		}
		if (bind(insock, p->ai_addr, p->ai_addrlen) < 0) {
			if (close(insock) < 0) {
				err(1, "close");
			}
			continue;
		}
		break;
	}
	freeaddrinfo(ai);
	if (!p) {
		err(1, "bind");
	}

	if (listen(insock, SOMAXCONN) < 0) {
		err(1, "listen");
	}

	return insock;
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
sock_get_inaddr_str(const struct sockaddr_storage *in_sa, char *str, size_t len) {
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
