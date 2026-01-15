/* miniquark: see LICENSE file for copyright and license details. */

#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "http.h"
#include "sock.h"

struct server {
	char *host;
	char *port;
} s;

static void
serve(int infd, struct sockaddr_storage *in_sa) {
	struct request r;
	enum status status;
	char inaddr[INET6_ADDRSTRLEN /* > INET_ADDRSTRLEN */];

	/* set connection timeout */
	if (sock_set_timeout(infd, 30)) {
		goto cleanup;
	}

	/* handle request */
	if (!(status = http_get_request(infd, &r))) {
		status = http_send_response(infd, &r);
	}

	if (inaddr_to_str(in_sa, inaddr, LEN(inaddr))) {
		goto cleanup;
	}
	printf("%lu\t%s\t%d\t%s\t%s\n", r.bytes_sent, inaddr, status, r.field[REQ_AGENT], r.target);

cleanup:
	/* clean up and finish */
	shutdown(infd, SHUT_RD);
	shutdown(infd, SHUT_WR);
	close(infd);
}

static void
sigcleanup(int sig) {
	kill(0, sig);
	_exit(1);
}

static void
handlesignals(void (*hdl)(int)) {
	struct sigaction sa = {
		.sa_handler = hdl,
	};

	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
}

static void
usage(void) {
	fprintf(stderr, "release: %s\n", RELEASE);
	fprintf(stderr, "usage: miniquark -p port [-h host] [-d dir]\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int ch;
	struct sockaddr_storage in_sa;
	socklen_t in_sa_len;
	int status = 0, infd;
	int addr_count, nready, n;
	char inaddr[INET6_ADDRSTRLEN];
	struct pollfd pfd[LISTEN_MAX];
	struct sockaddr_storage resolved[LISTEN_MAX];

	/* defaults */
	char *servedir = ".";

	s.host = "localhost";
	s.port = NULL;

	opterr = 0;
	while ((ch = getopt(argc, argv, "p:h:d:")) != -1) {
		switch (ch) {
		case 'd':
			servedir = optarg;
			break;
		case 'h':
			s.host = optarg;
			break;
		case 'p':
			s.port = optarg;
			break;
		default:
			usage();
		}
	}
	if (argc > optind + 1)
		usage();
	if (s.port == NULL)
		usage();

	/* Open a new process group */
	setpgid(0, 0);

	handlesignals(sigcleanup);

	switch (fork()) {
	case -1:
		warn("fork");
		break;
	case 0:
		/* restore default handlers */
		handlesignals(SIG_DFL);

		/* reap children automatically */
		if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
			err(1, "Failed to set SIG_IGN on SIGCHLD");
		}

		if (chdir(servedir) < 0)
			err(1, "chdir '%s'", servedir);

		addr_count = addr_listen(s.host, s.port, pfd, resolved);
		printf("Listening on");
		for (n = 0; n < addr_count; n++) {
			inaddr_to_str(&resolved[n], inaddr, sizeof(inaddr));
			printf(" %s", inaddr);
		}
		printf(" port %s\n", s.port);
		fflush(stdout);

		/* accept incoming connections */
		while (1) {
			infd = 0;
			nready = poll(pfd, addr_count, -1);
			if (nready < 1)
				err(1, "poll");

			for (n = 0; n < addr_count; n++) {
				if (pfd[n].revents & (POLLERR | POLLNVAL))
					errx(1, "bad fd %d", pfd[n].fd);
				if (pfd[n].revents & (POLLIN | POLLHUP)) {
					in_sa_len = sizeof(in_sa);
					infd = accept(pfd[n].fd, (struct sockaddr *) &in_sa, &in_sa_len);
					if (infd < 0)
						err(1, "accept");
				}
			}
			if (!infd)
				continue;

			/* fork and handle */
			switch (fork()) {
			case 0:
				serve(infd, &in_sa);
				exit(0);
				break;
			case -1:
				warn("fork");
				/* fallthrough */
			default:
				/* close the connection in the parent */
				close(infd);
			}
		}
		exit(0);
	default:
		while (wait(&status) > 0)
			;
	}

	return status;
}
