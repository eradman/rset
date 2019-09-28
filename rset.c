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

#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"
#include "config.h"
#include "rutils.h"
#include "execute.h"

/* forwards */

static void handle_exit(int sig);
static void usage();
static void not_found(char *name);

/* globals used by input.l */

FILE* yyin;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int list_opt;
int dryrun_opt;

/* globals used by signal handlers */
char *socket_path;
char *hostname;
int http_port;

int main(int argc, char *argv[])
{
	char buf[_POSIX2_LINE_MAX];
	int ch;
	int i, j;
	int rv;
	int fd;
	pid_t http_server_pid;
	pid_t rset_pid;
	int status;
	char *host_pattern, *label_pattern;
	char *http_srv_argv[9], *inputstring;
	char *rinstall_bin, *rsub_bin, *httpd_bin;
	char routes_realpath[PATH_MAX];
	regmatch_t regmatch;
	regex_t host_reg, label_reg;
	sigset_t set;
	struct sigaction act;
	Options toplevel_options;

	int labels_matched = 0;
	char *routes_file = ROUTES_FILE;
	char *sshconfig_file = NULL;

	opterr = 0;
	while ((ch = getopt(argc, argv, "lnvF:f:")) != -1)
		switch (ch) {
		case 'l':
			list_opt += 1;
			break;
		case 'n':
			dryrun_opt = 1;
			break;
		case 'F':
			sshconfig_file = argv[optind-1];
			break;
		case 'f':
			routes_file = argv[optind-1];
			break;
		default:
			usage();
	}
	if (optind >= argc) usage();
	if (argc > optind+2) usage();

	host_pattern = argv[optind];
	label_pattern = argv[optind+1];
	if (!label_pattern)
		label_pattern = DEFAULT_LABEL_PATTERN;
	if ((rinstall_bin = findprog("rinstall")) == 0)
		not_found("rinstall");
	if ((rsub_bin = findprog("rsub")) == 0)
		not_found("rsub");

	/* all operations must be relative to the routes file */
	if (realpath(xdirname(routes_file), routes_realpath) == NULL)
		err(1, "realpath %s", routes_file);
	if (chdir(routes_realpath) == -1)
		err(1, "chdir %s", routes_realpath);
	routes_file = basename(routes_file);

	/* try opening the routes file */
	if ((fd = open(routes_file, O_RDONLY)) == -1)
		err(1, "unable to open %s", routes_file);
	(void) close(fd);

	if (!dryrun_opt) {
		/* Auto-upgrade utilities and verify path */
		install_if_new(rinstall_bin, REPLICATED_DIRECTORY "/rinstall");
		install_if_new(rsub_bin, REPLICATED_DIRECTORY "/rsub");
		create_dir(PUBLIC_DIRECTORY);
	}

	/* Select a port to communicate on */
	http_port = get_socket();

	if (pledge("stdio rpath proc exec unveil", NULL) == -1)
		err(1, "pledge");

	/* Convert http server command line into a vector */
	inputstring = malloc(PATH_MAX);
	snprintf(inputstring, PATH_MAX, WEB_SERVER, http_port);
	str_to_array(http_srv_argv, inputstring, sizeof(http_srv_argv));
	if ((httpd_bin = findprog(http_srv_argv[0])) == 0)
		not_found(http_srv_argv[0]);

	/* start the web server */
	http_server_pid = fork();
	if (http_server_pid == 0) {
		if (pledge("stdio rpath proc exec unveil", "stdio rpath inet") == -1)
			err(1, "pledge");

		/* elide startup notices */
		close(STDOUT_FILENO);

		unveil(xdirname(PUBLIC_DIRECTORY), "r");
		unveil(xdirname(httpd_bin), "x");
		unveil("/usr/lib", "r");
		unveil("/usr/libexec", "r");

		execv(httpd_bin, http_srv_argv);
		fprintf(stderr, "Fatal: unable to start web server\n");
		err(1, "%s", httpd_bin);
	}

	/* watchdog to ensure that the http server is shut down*/
	rset_pid = fork();
	if (rset_pid > 0) {
		if (pledge("stdio proc", NULL) == -1)
			err(1, "pledge");

		setproctitle("watch on pid %d", rset_pid);
		sigfillset(&set);
		sigprocmask(SIG_BLOCK, &set, NULL);
		if (waitpid(rset_pid, &status, 0) == -1)
			warn("wait on rset with pid %d", rset_pid);
		if (kill(http_server_pid, SIGTERM) == -1)
			err(1, "terminate http_server with pid %d", http_server_pid);
		exit(WEXITSTATUS(status));
	}

	/* terminate SSH connection if a signal is caught */
	act.sa_flags = 0;
	act.sa_flags = SA_RESETHAND;
	act.sa_handler = handle_exit;
	if (sigemptyset(&act.sa_mask) & (sigaction(SIGINT, &act, NULL) != 0))
		err(1, "Failed to set SIGINT handler");
	if (sigemptyset(&act.sa_mask) & (sigaction(SIGTERM, &act, NULL) != 0))
		err(1, "Failed to set SIGTERM handler");

	/* parse route labels */
	n_labels = 0;
	route_labels = alloc_labels();
	host_labels = route_labels;
	yyin = fopen(routes_file, "r");
	if (!yyin)
		err(1, "%s", routes_file);
	yylex();
	fclose(yyin);

	if ((rv = regcomp(&host_reg, host_pattern, REG_EXTENDED)) != 0) {
		regerror(rv, &host_reg, buf, sizeof(buf));
		errx(1, "bad expression: %s", buf);
	}

	if ((rv = regcomp(&label_reg, label_pattern, REG_EXTENDED)) != 0) {
		regerror(rv, &label_reg, buf, sizeof(buf));
		errx(1, "bad expression: %s", buf);
	}

	/* parse pln files for each host */
	for (i=0; route_labels[i]; i++) {
		memcpy(&toplevel_options, &current_options, sizeof(toplevel_options));
		read_host_labels(route_labels[i]);
		memcpy(&current_options, &toplevel_options, sizeof(current_options));
	}
	if (dryrun_opt) goto dry_run;

	/* execute commands on remote hosts */
	for (i=0; route_labels[i]; i++) {
		hostname = route_labels[i]->name;
		host_labels = route_labels[i]->labels;
		rv = regexec(&host_reg, hostname, 1, &regmatch, 0);
		if (rv == 0) {
			if (list_opt > 1) {
					snprintf(buf, sizeof(buf), "%-20s", hostname);
					hl_range(buf, HL_HOST, regmatch.rm_so, regmatch.rm_eo);
					printf("  %s\n", array_to_str(route_labels[i]->export_paths));
			}
			else {
				hl_range(hostname, HL_HOST, regmatch.rm_so, regmatch.rm_eo);
				printf("\n");
			}
			socket_path = start_connection(route_labels[i], http_port,
			    sshconfig_file);
			if (socket_path == NULL)
				continue;
			for (j=0; host_labels[j]; j++) {
				rv = regexec(&label_reg, host_labels[j]->name, 1, &regmatch, 0);
				if (rv != 0)
					continue;
				if (list_opt > 1) {
					snprintf(buf, sizeof(buf), "%-20s", host_labels[j]->name);
					hl_range(buf, HL_LABEL, 0, 0);
					printf("  %s\n", format_options(&host_labels[j]->options));
				}
				else if (list_opt) {
					hl_range(host_labels[j]->name, HL_LABEL, 0, 0);
					printf("\n");
				}
				(void) ssh_command(hostname, socket_path, host_labels[j], http_port);
			}
			end_connection(socket_path, hostname, http_port);
			socket_path = NULL;
		}
	}
	return 0;

dry_run:
	for (i=0; route_labels[i]; i++) {
		hostname = route_labels[i]->name;
		host_labels = route_labels[i]->labels;
		rv = regexec(&host_reg, hostname, 1, &regmatch, 0);
		if (rv == 0) {
			if (list_opt > 1) {
					snprintf(buf, sizeof(buf), "%-20s", hostname);
					hl_range(buf, HL_HOST, regmatch.rm_so, regmatch.rm_eo);
					printf("  %s\n", array_to_str(route_labels[i]->export_paths));
			}
			else {
				hl_range(hostname, HL_HOST, regmatch.rm_so, regmatch.rm_eo);
				printf("\n");
			}
			for (j=0; host_labels[j]; j++) {
				rv = regexec(&label_reg, host_labels[j]->name, 1, &regmatch, 0);
				if (rv != 0)
					continue;
				labels_matched++;
				if (list_opt > 1) {
					snprintf(buf, sizeof(buf), "%-20s", host_labels[j]->name);
					hl_range(buf, HL_LABEL, regmatch.rm_so, regmatch.rm_eo);
					printf("  %s\n", format_options(&host_labels[j]->options));
				}
				else if (list_opt) {
					hl_range(host_labels[j]->name, HL_LABEL, regmatch.rm_so, regmatch.rm_eo);
					printf("\n");
				}
				if (dryrun_opt)
					continue;
			}
		}
	}
	if (labels_matched == 0)
		errx(1, "no matching labels found");
	return 0;
}

/* signal handlers */

void
handle_exit(int sig) {
	if (socket_path && hostname && http_port) {
		printf("caught signal %d, terminating connection to '%s'\n", sig,
			hostname);
		/* clean up socket and SSH connection; leaving staging dir */
		execlp("ssh", "ssh", "-S", socket_path, "-O", "exit", hostname, NULL);
		err(1, "ssh -O exit");
	}
}

/* internal utilty functions */

static void
usage() {
	fprintf(stderr, "release: %s\n", RELEASE);
	fprintf(stderr, "usage: rset [-lln] [-F sshconfig_file] [-f routes_file] "
	    "host_pattern [label_pattern]\n");
	exit(1);
}

static void
not_found(char *name) {
	fprintf(stderr, "rset: %s not found in PATH\n", name);
	exit(1);
}
