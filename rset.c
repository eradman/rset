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
#include <errno.h>
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
#include "input.h"

/* forwards */

static void handle_exit(int sig);
static void usage();
static void not_found(char *name);
static void format_http_log(char *output, size_t len);

/* globals used by input.h */

FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int list_opt;
int dryrun_opt;
int tty_opt;
int verbose_opt;
int stop_on_err_opt;

/* globals used by signal handlers */
char *socket_path;
char *hostname;
int http_port;

int main(int argc, char *argv[])
{
	char buf[_POSIX2_LINE_MAX];
	char httpd_log[32768];
	int ch;
	int fd;
	int flags;
	int i, j, k;
	int nr;
	int rv;
	int stdout_pipe[2];
	pid_t http_server_pid;
	pid_t rset_pid;
	int status;
	char *hostnames[ARG_MAX/8];
	char *http_srv_argv[9], *inputstring;
	char *rinstall_bin, *rsub_bin, *httpd_bin;
	char routes_realpath[PATH_MAX];
	regmatch_t regmatch;
	regex_t label_reg;
	sigset_t set;
	size_t len;
	struct sigaction act;
	Options toplevel_options, op;

	int exit_code = 0;
	int labels_matched = 0;
	char *label_pattern = DEFAULT_LABEL_PATTERN;
	char *routes_file = ROUTES_FILE;
	char *sshconfig_file = NULL;

	opterr = 0;
	while ((ch = getopt(argc, argv, "elntvF:f:x:")) != -1) {
		switch (ch) {
		case 'e':
			stop_on_err_opt = 1;
			break;
		case 'l':
			list_opt = 1;
			break;
		case 'n':
			dryrun_opt = 1;
			break;
		case 't':
			tty_opt = 1;
			break;
		case 'v':
			verbose_opt = 1;
			break;
		case 'F':
			sshconfig_file = argv[optind-1];
			break;
		case 'f':
			routes_file = argv[optind-1];
			break;
		case 'x':
			label_pattern = argv[optind-1];
			break;

		default:
			usage();
		}
	}
	if (optind >= argc) usage();

	for (i=0; i < argc - optind; i++)
		hostnames[i] = argv[optind+i];
	hostnames[i] = NULL;

	if ((rinstall_bin = findprog("rinstall")) == 0)
		not_found("rinstall");
	if ((rsub_bin = findprog("rsub")) == 0)
		not_found("rsub");

	/* all operations must be relative to the routes file */
	if (realpath(xdirname(routes_file), routes_realpath) == NULL)
		err(1, "realpath %s", routes_file);
	if (chdir(routes_realpath) == -1)
		err(1, "chdir %s", routes_realpath);

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

	if (pledge("stdio rpath proc exec unveil tmppath", NULL) == -1)
		err(1, "pledge");

	/* Convert http server command line into a vector */
	inputstring = malloc(PATH_MAX);
	snprintf(inputstring, PATH_MAX, "miniquark -p %d -d " PUBLIC_DIRECTORY, http_port);
	str_to_array(http_srv_argv, inputstring, sizeof(http_srv_argv));
	if ((httpd_bin = findprog(http_srv_argv[0])) == 0)
		not_found(http_srv_argv[0]);

	/* start the web server */
	pipe(stdout_pipe);
	http_server_pid = fork();
	if (http_server_pid == 0) {
		/* close input side of pipe, and connect stdout */
		dup2(stdout_pipe[1], STDOUT_FILENO);
		close(stdout_pipe[1]);

		if (unveil(xdirname(PUBLIC_DIRECTORY), "r") == -1)
			err(1, "unveil");
		if (unveil(xdirname(httpd_bin), "x") == -1)
			err(1, "unveil");
		if (unveil("/usr/lib", "r") == -1)
			err(1, "unveil");
		if (unveil("/usr/libexec", "r") == -1)
			err(1, "unveil");
		if (pledge("stdio rpath proc exec", "stdio rpath proc inet") == -1)
			err(1, "pledge");


		execv(httpd_bin, http_srv_argv);
		fprintf(stderr, "Fatal: unable to start web server\n");
		err(1, "%s", httpd_bin);
	}

	/* close output side of pipe, and ensure readers don't block */
	close(stdout_pipe[1]);
	flags = fcntl(stdout_pipe[0], F_GETFL);
	fcntl(stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

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
	yyfn = routes_file;
	yyin = fopen(routes_file, "r");
	if (!yyin)
		err(1, "%s", routes_file);
	yylex();
	fclose(yyin);

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

	/* find arguments that don't match */
	for (k=0; hostnames[k]; k++) {
		rv = 1;
		for (i=0; route_labels[i]; i++) {
			rv = strcmp(hostnames[k], route_labels[i]->name);
			if (rv == 0) break;
		}
		if (rv != 0) {
			fprintf(stderr, "No match for '%s' in %s\n", hostnames[k], routes_file);
			exit(1);
		}
	}

	if (dryrun_opt) goto dry_run;

#ifdef REQUIRE_SSH_AGENT
	if (verify_ssh_agent() != 0) {
		printf("Try running:\n");
			if (!getenv("SSH_AUTH_SOCK"))
				printf("  eval `ssh-agent`\n");
			printf("  ssh-add\n");
		exit(1);
	}
#endif

	/* execute commands on remote hosts */
	for (i=0; route_labels[i]; i++) {
		hostname = route_labels[i]->name;
		host_labels = route_labels[i]->labels;

		rv = 1;
		for (k=0; hostnames[k]; k++) {
			rv = strcmp(hostnames[k], hostname);
			if (rv == 0) break;
		}

		if (rv == 0) {
			if (list_opt) {
					snprintf(buf, sizeof(buf), "%-20s", hostname);
					hl_range(buf, HL_HOST, 0, 0);
					printf("  %s\n", array_to_str(route_labels[i]->export_paths));
			}
			else {
				hl_range(hostname, HL_HOST, 0, 0);
				printf("\n");
			}

			len = PLN_LABEL_SIZE + sizeof(LOCAL_SOCKET_PATH);
			socket_path = malloc(len);
			snprintf(socket_path, len, LOCAL_SOCKET_PATH, route_labels[i]->name);

			if (start_connection(socket_path, route_labels[i], http_port, sshconfig_file) == -1) {
				end_connection(socket_path, hostname, http_port);
				free(socket_path);
				socket_path = NULL;
				continue;
			}
			for (j=0; host_labels[j]; j++) {
				rv = regexec(&label_reg, host_labels[j]->name, 1, &regmatch, 0);
				if (rv != 0)
					continue;
				if (list_opt) {
					snprintf(buf, sizeof(buf), "%-20s", host_labels[j]->name);
					hl_range(buf, HL_LABEL, 0, 0);
					printf("  %s\n", format_options(&host_labels[j]->options));
				}
				else {
					hl_range(host_labels[j]->name, HL_LABEL, 0, 0);
					printf("\n");
				}

				if (tty_opt)
					exit_code = ssh_command_tty(hostname, socket_path, host_labels[j], http_port);
				else
					exit_code = ssh_command_pipe(hostname, socket_path, host_labels[j], http_port);
				
				if ((stop_on_err_opt) && exit_code != 0) {
					apply_default(op.interpreter, host_labels[j]->options.interpreter, INTERPRETER);
					snprintf(buf, sizeof(buf), "%s exited with code %d", op.interpreter, exit_code);
					hl_range(buf, HL_ERROR, 0, 0);
					printf("\n");

					goto exit;
				}

				/* read output of web server */
				nr = read(stdout_pipe[0], httpd_log, sizeof(httpd_log));
				if ((verbose_opt) && (nr > 0))
					format_http_log(httpd_log, nr);
				if ((nr == -1) && (errno != EAGAIN))
					warn("read from httpd output");
			}

exit:
			if (socket_path) {
				end_connection(socket_path, hostname, http_port);
				free(socket_path);
				socket_path = NULL;
			}
		}
	}

	if (stop_on_err_opt)
		return exit_code;
	else
		return 0;

dry_run:
	for (i=0; route_labels[i]; i++) {
		hostname = route_labels[i]->name;
		host_labels = route_labels[i]->labels;

		rv = 1;
		for (k=0; hostnames[k]; k++) {
			rv = strcmp(hostnames[k], hostname);
			if (rv == 0) break;
		}

		if (rv == 0) {
			if (list_opt) {
					snprintf(buf, sizeof(buf), "%-20s", hostname);
					hl_range(buf, HL_HOST, 0, strlen(hostname));
					printf("  %s\n", array_to_str(route_labels[i]->export_paths));
			}
			else {
				hl_range(hostname, HL_HOST, 0, strlen(hostname));
				printf("\n");
			}
			for (j=0; host_labels[j]; j++) {
				rv = regexec(&label_reg, host_labels[j]->name, 1, &regmatch, 0);
				if (rv != 0)
					continue;
				labels_matched++;
				if (list_opt) {
					snprintf(buf, sizeof(buf), "%-20s", host_labels[j]->name);
					hl_range(buf, HL_LABEL, regmatch.rm_so, regmatch.rm_eo);
					printf("  %s\n", format_options(&host_labels[j]->options));
				}
				else {
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
	fprintf(stderr, "usage: rset [-elntv] [-F sshconfig_file] [-f routes_file] "
	    "[-x label_pattern] hostname ...\n");
	exit(1);
}

static void
not_found(char *name) {
	errx(1, "'%s' not found in PATH", name);
}

static void
format_http_log(char *output, size_t len) {
	char *eol, *output_start;
	static int n_lines = 0;

	output[len] = '\0';
	output_start = output;

	while (n_lines++ < 0) {  /* 1 to strip off startup message */
		if ((eol = strchr(output_start, '\n')) != NULL) {
			*eol = '\0';
			output_start = eol+1;
		}
	}
	printf("%s", output_start);
	if (output[len-1] != '\n')
			printf(" ...\n");
}
