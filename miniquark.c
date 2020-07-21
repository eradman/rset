/* miniquark: see LICENSE file for copyright and license details. */

#include <err.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "http.h"
#include "sock.h"

struct server {
	char *host;
	char *port;
} s;

static void
serve(int infd, struct sockaddr_storage *in_sa)
{
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

	if (sock_get_inaddr_str(in_sa, inaddr, LEN(inaddr))) {
		goto cleanup;
	}
	printf("%lu\t%s\t%d\t%s\t%s\n", r.bytes_sent, inaddr, status,  r.field[REQ_AGENT], r.target);

cleanup:
	/* clean up and finish */
	shutdown(infd, SHUT_RD);
	shutdown(infd, SHUT_WR);
	close(infd);
}

static void
sigcleanup(int sig)
{
	kill(0, sig);
	_exit(1);
}

static void
handlesignals(void(*hdl)(int))
{
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
usage(void)
{
	fprintf(stderr, "release: %s\n", RELEASE);
	fprintf(stderr, "usage: miniquark -p port [-h host] [-d dir]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char ch;
	struct sockaddr_storage in_sa;
	pid_t cpid, wpid, spid;
	socklen_t in_sa_len;
	int insock, status = 0, infd;

	/* defaults */
	char *servedir = ".";

	s.host = "127.0.0.1";
	s.port = NULL;

	opterr = 0;
	while ((ch = getopt(argc, argv, "p:h:d:")) != -1)
		switch (ch) {
		case 'd':
			servedir = argv[optind-1];
			break;
		case 'h':
			s.host = argv[optind-1];
			break;
		case 'p':
			s.port = argv[optind-1];
			break;
		default:
			usage();
	}
	if (argc > optind+1) usage();
	if (s.port == NULL) usage();

	/* Open a new process group */
	setpgid(0,0);

	handlesignals(sigcleanup);
	printf("listening on: http://%s:%s/\n", s.host, s.port);
	fflush(stdout);

	switch (cpid = fork()) {
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

		/* chroot */
		if (getuid() == 0) {
			if (chroot(".") < 0) {
				err(1, "chroot .");
			}
		}
		insock = sock_get_ips(s.host, s.port);

		/* accept incoming connections */
		while (1) {
			in_sa_len = sizeof(in_sa);
			if ((infd = accept(insock, (struct sockaddr *)&in_sa,
			                   &in_sa_len)) < 0) {
				warn("accept");
				continue;
			}

			/* fork and handle */
			switch ((spid = fork())) {
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
		while ((wpid = wait(&status)) > 0)
			;
	}

	return status;
}
