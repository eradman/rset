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

#include "config.h"
#include "execute.h"

/* forwards */

static void usage();

/* globals used by input.l */

FILE* yyin;
int yylex();
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

/* globals */
int verbose_opt;


int main(int argc, char *argv[])
{
	char buf[_POSIX2_LINE_MAX];
	int ch;
	int i, j;
	int rv;
	int http_port;
	pid_t http_server_pid;
	char *cmd, *socket_path;
	char *host_pattern, *host_name;
	char *http_srv_argv[9], *inputstring;
	regmatch_t regmatch;
	regex_t reg;
	Options toplevel_options;

	opterr = 0;
	while ((ch = getopt(argc, argv, "v")) != -1)
		switch (ch) {
		case 'v':
			verbose_opt = 1;
			break;
	}

	if (optind >= argc) usage();
	host_pattern = argv[optind];

	/* Auto-upgrade utilities and verify path */
	snprintf(buf, sizeof(buf), "%s/rinstall", dirname(argv[0]));
	install_if_new(buf, UTILITIES_TO_SEND "/rinstall");

	/* Select a port to communicate on */
	http_port = get_socket();

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

			printf(ANSI_YELLOW "%s" ANSI_RESET "\n", host_name);
			socket_path = start_connection(host_name, http_port);
			for (j=0; host_labels[j]; j++) {
				if (verbose_opt)
					printf(ANSI_CYAN "%s" ANSI_RESET "\n", host_labels[j]->name);
				cmd = ssh_command(host_name, socket_path, host_labels[j]->name,
				    &host_labels[j]->options, http_port);
				pipe_cmd(cmd, host_labels[j]->content, host_labels[j]->content_size);
				free(cmd);
			}
			end_connection(socket_path, host_name, http_port);

			memcpy(&current_options, &toplevel_options, sizeof(current_options));
		}
	}

	kill(http_server_pid, SIGTERM);
	return 0;
}

/* Utility functions */

void
usage() {
	fprintf(stderr, "release: %s\n", RELEASE);
	fprintf(stderr, "usage: rset [-v] host_pattern\n");
	exit(1);
}
