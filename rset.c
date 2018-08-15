/*
 * Copyright (c) 2018 Eric Radman <ericshane@eradman.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <libgen.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

#include "config.h"
#include "execute.h"

/* forwards */

static void usage();
static void hl_line(const char *s, int t);
static void hl_range(const char *s, unsigned so, unsigned eo);

/* globals used by input.l */

FILE* yyin;
int yylex();
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int list_opt;
int dryrun_opt;

int main(int argc, char *argv[])
{
	char buf[_POSIX2_LINE_MAX];
	int ch;
	int i, j;
	int rv;
	int labels_matched = 0;
	int http_port;
	pid_t http_server_pid;
	pid_t rset_pid;
	int status;
	char *socket_path;
	char *selected_label;
	char *host_pattern, *host_name;
	char *http_srv_argv[9], *inputstring;
	regmatch_t regmatch;
	regex_t reg;
	sigset_t set;
	Options toplevel_options;

	opterr = 0;
	while ((ch = getopt(argc, argv, "ln")) != -1)
		switch (ch) {
		case 'l':
			list_opt = 1;
			break;
		case 'n':
			dryrun_opt = 1;
			break;
		default:
			usage();
	}
	if (optind >= argc) usage();
	if (argc > optind+2) usage();

	host_pattern = argv[optind];
	selected_label = argv[optind+1];

	/* Select a port to communicate on */
	http_port = get_socket();

	if (!dryrun_opt) {
		/* Auto-upgrade utilities and verify path */
		snprintf(buf, sizeof(buf), "%s/rinstall", dirname(argv[0]));
		install_if_new(buf, REPLICATED_DIRECTORY "/rinstall");
	}

	/* start the web server */
	http_server_pid = fork();
	if (http_server_pid == 0) {
		inputstring = malloc(PATH_MAX);
		snprintf(inputstring, PATH_MAX, WEB_SERVER, dirname(TOP_LEVEL_ROUTE_FILE), http_port);
		/* elide copyright and other startup notices */
		close(STDOUT_FILENO);
		/* Convert http server command line into a vector */
		str_to_array(http_srv_argv, inputstring, sizeof(http_srv_argv));
		execvp(http_srv_argv[0], http_srv_argv);
		err(1, "%s", http_srv_argv[0]);
	}
	/* watchdog to ensure that the http server is shut down*/
	rset_pid = fork();
	if (rset_pid > 0) {
		setproctitle("watch on pid %d", rset_pid);
		sigfillset(&set);
		sigprocmask(SIG_BLOCK, &set, NULL);
		if (waitpid(rset_pid, &status, 0) == -1)
			warn("wait on rset with pid %d", rset_pid);
		if (kill(http_server_pid, SIGTERM) == -1)
			err(1, "terminate http_server with pid %d", http_server_pid);
		exit(WEXITSTATUS(status));
	}

	/* parse route labels */
	n_labels = 0;
	route_labels = alloc_labels();
	host_labels = route_labels;
	yyin = fopen(TOP_LEVEL_ROUTE_FILE, "r");
	if (!yyin)
		err(1, "%s", TOP_LEVEL_ROUTE_FILE);
	yylex();
	fclose(yyin);

	if ((rv = regcomp(&reg, host_pattern, REG_EXTENDED)) != 0) {
		regerror(rv, &reg, buf, sizeof(buf));
		errx(1, "bad expression: %s", buf);
	}

	for (i=0; route_labels[i]; i++) {
		host_name = route_labels[i]->name;
		rv = regexec(&reg, host_name, 1, &regmatch, 0);
		if (rv == 0) {
			memcpy(&toplevel_options, &current_options, sizeof(toplevel_options));

			read_host_labels(route_labels[i]);
			if (!host_labels[0])
				continue;

			if (dryrun_opt)
				hl_range(host_name, regmatch.rm_so, regmatch.rm_eo);
			else {
				hl_line(host_name, 1);
				socket_path = start_connection(host_name, http_port);
				if (socket_path == NULL)
					continue;
			}
			for (j=0; host_labels[j]; j++) {
				if (selected_label)
					if (strcmp(selected_label, host_labels[j]->name) != 0)
						continue;
				labels_matched++;
				if (list_opt)
					hl_line(host_labels[j]->name, 2);
				if (dryrun_opt)
					continue;
				else
					rv = ssh_command(host_name, socket_path, host_labels[j], http_port);
			}
			if (!dryrun_opt)
				end_connection(socket_path, host_name, http_port);

			memcpy(&current_options, &toplevel_options, sizeof(current_options));
		}
	}

	if (selected_label && labels_matched == 0)
		errx(1, "no matching labels found");

	return 0;
}

/* Utility functions */

void
usage() {
	fprintf(stderr, "release: %s\n", RELEASE);
	fprintf(stderr, "usage: rset [-ln] host_pattern [label]\n");
	exit(1);
}

void
hl_line(const char *s, int t) {
	switch (t) {
		case 1:
			printf(ANSI_YELLOW "%s" ANSI_RESET "\n", s);
			break;
		case 2:
			printf(ANSI_CYAN "%s" ANSI_RESET "\n", s);
			break;
		default:
			printf("%s\n", s);
	}
}

void
hl_range(const char *s, unsigned so, unsigned eo) {
	char *start, *match;

	start = strndup(s, so);
	match = strndup(s+so, eo-so);

	printf(ANSI_YELLOW "%s" ANSI_REVERSE "%s" ANSI_RESET ANSI_YELLOW "%s"
	    ANSI_RESET "\n", start, match, s+eo);
	free(start); free(match);
}
