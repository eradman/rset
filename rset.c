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
#include "execute.h"
#include "input.h"
#include "rutils.h"
#include "worker.h"
#include "xlibc.h"

/* forwards */
static void handle_exit(int sig);
static void usage();
static void set_options(int argc, char *argv[], char *hostnames[]);
static void not_found(char *name);
static void start_http_server(int stdout_pipe[], int http_port);
static void compare_argv_routes(char *hostnames[], Label **route_labels);
static int execute_remote(char *hostnames[], Label **route_labels, regex_t *label_reg);
static int dry_run(char *hostnames[], Label **route_labels, regex_t *label_reg);

/* globals from input.h */
Label **route_labels;

/* globals */
int archive_opt;
int dryrun_opt;
int restore_opt;
int tty_opt;
int stop_on_err_opt;
int n_parallel;
char *sshconfig_file;
char *env_override;
char *log_directory;
char *label_pattern = DEFAULT_LABEL_PATTERN;
char *routes_file = ROUTES_FILE;

/* globals used by signal handlers */
char *socket_path;
char *hostname;
int http_port;

/*
 * Remote Staging Execution Tool
 * configure systems using any scripting language
 */
int
main(int argc, char *argv[]) {
	char buf[_POSIX2_LINE_MAX];
	int fd;
	int i, j;
	int ret;
	int rv;
	int n_workers;
	int worker_argc[MAX_WORKERS];
	int worker_pid[MAX_WORKERS];
	char *renv_bin, *rinstall_bin, *rsub_bin;
	char **hostnames;
	char **worker_argv[MAX_WORKERS];
	char routes_realpath[PATH_MAX];
	regex_t label_reg;
	struct sigaction act;

	/* terminate SSH connection if a signal is caught */
	act.sa_flags = 0;
	act.sa_flags = SA_RESETHAND;
	act.sa_handler = handle_exit;
	if (sigemptyset(&act.sa_mask) & (sigaction(SIGINT, &act, NULL) != 0))
		err(1, "Failed to set SIGINT handler");
	if (sigemptyset(&act.sa_mask) & (sigaction(SIGTERM, &act, NULL) != 0))
		err(1, "Failed to set SIGTERM handler");

	hostnames = xcalloc(argc, sizeof(char *), "hostnames");
	set_options(argc, argv, hostnames);

	if ((renv_bin = findprog("renv")) == 0)
		not_found("renv");
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

	check_permissions(routes_realpath);

	if (!dryrun_opt) {
		/* require ssh-agent */
		if (verify_ssh_agent() != 0) {
			printf("Try running:\n");
			if (!getenv("SSH_AUTH_SOCK"))
				printf("  eval `ssh-agent`\n");
			printf("  ssh-add\n");
			exit(1);
		}

		/* Auto-upgrade utilities and verify path */
		install_if_new(renv_bin, REPLICATED_DIRECTORY "/renv");
		install_if_new(rinstall_bin, REPLICATED_DIRECTORY "/rinstall");
		install_if_new(rsub_bin, REPLICATED_DIRECTORY "/rsub");
		create_dir(ARCHIVE_DIRECTORY);
		create_dir(PUBLIC_DIRECTORY);
	}

	/* parse route labels */
	route_labels = alloc_labels();
	read_route_labels(routes_file);

	if ((rv = regcomp(&label_reg, label_pattern, REG_EXTENDED)) != 0) {
		regerror(rv, &label_reg, buf, sizeof(buf));
		errx(1, "bad expression: %s", buf);
	}

	/* parse pln files for each host */
	for (i = 0; route_labels[i]; i++)
		read_host_labels(route_labels[i]);

	/* ensure hostnames are valid */
	compare_argv_routes(hostnames, route_labels);

	if (n_parallel > 0) {
		n_workers = 0;
		create_dir(log_directory);

		/* distribute hostnames */
		for (i = 0; hostnames[i]; i++) {
			j = i % n_parallel;
			if (j + 1 > n_workers) {
				worker_argv[n_workers] = xcalloc(argc, sizeof(char *), "worker_argv[]");
				worker_argc[n_workers] = create_worker_argv(argv, worker_argv[n_workers]);
				n_workers++;
			}
			worker_argv[j][worker_argc[j]++] = hostnames[i];
		}

		for (i = 0; i < n_workers; i++)
			worker_pid[i] = exec_worker(log_directory, i + 1, worker_argv[i]);

		rexec_summary(n_workers, worker_pid, log_directory);
		exit(0);
	}

	/* select a port to communicate on */
	http_port = get_socket();

	if (pledge("stdio rpath proc exec unveil tmppath", NULL) == -1)
		err(1, "pledge");

	/* main loop */
	if (dryrun_opt) {
		ret = dry_run(hostnames, route_labels, &label_reg);
		free(hostnames);
		return ret;
	}

	ret = execute_remote(hostnames, route_labels, &label_reg);
	free(hostnames);
	return ret;
}

/*
 * Execute commands on remote hosts
 * Returns an exit status
 */

static int
execute_remote(char *hostnames[], Label **route_labels, regex_t *label_reg) {
	char httpd_log[32768];
	int i, j, k, l;
	int nr;
	int ret;
	int stdout_pipe[2];
	size_t len;
	regmatch_t regmatch;
	Label **host_labels;

	int exit_code = 0;
	int local_exit_code = 0;
	int scp_exit_code = 0;

	char *host_connect_msg = HL_HOST "%h" HL_RESET;
	char *host_connect_error_msg = HL_ERROR "%h initialization error" HL_RESET;
	char *label_exec_begin_msg = HL_LABEL "%l" HL_RESET;
	char *label_exec_end_msg = 0;
	char *label_exec_error_msg = HL_ERROR "%l exited with code %e" HL_RESET;
	char *host_disconnect_msg = 0;

	/* start background web server */
	start_http_server(stdout_pipe, http_port);

	/* custom log format */
	if (getenv("RSET_HOST_CONNECT")) {
		host_connect_msg = getenv("RSET_HOST_CONNECT");
		host_connect_error_msg = getenv("RSET_HOST_CONNECT_ERROR");
		label_exec_begin_msg = getenv("RSET_LABEL_EXEC_BEGIN");
		label_exec_end_msg = getenv("RSET_LABEL_EXEC_END");
		host_disconnect_msg = getenv("RSET_HOST_DISCONNECT");
		label_exec_error_msg = getenv("RSET_LABEL_EXEC_ERROR");
	}

	for (i = 0; route_labels[i]; i++) {
		host_labels = route_labels[i]->labels;

		for (k = 0; hostnames[k]; k++) {
			for (l = 0; l < route_labels[i]->n_aliases; l++) {
				if (strcmp(hostnames[k], route_labels[i]->aliases[l]) == 0)
					hostname = route_labels[i]->aliases[l];
				else
					continue;

				generate_session_id();
				log_msg(host_connect_msg, hostname, "", 0);

				len = PLN_LABEL_SIZE + sizeof(LOCAL_SOCKET_PATH);
				socket_path = xmalloc(len, "socket_path");
				snprintf(socket_path, len, LOCAL_SOCKET_PATH, hostname);

				ret = start_connection(
				    socket_path, hostname, route_labels[i], http_port, sshconfig_file);
				if (ret != 0) {
					log_msg(host_connect_error_msg, hostname, "", ret);
					end_connection(socket_path, hostname, http_port);
					free(socket_path);
					socket_path = NULL;
					continue;
				}

				for (j = 0; host_labels[j]; j++) {
					if (regexec(label_reg, host_labels[j]->name, 1, &regmatch, 0) != 0)
						continue;

					log_msg(label_exec_begin_msg, hostname, host_labels[j]->name, 0);

					/* local begin */
					local_exit_code = local_exec(host_labels[j], host_labels[j]->options.begin);

					if (stop_on_err_opt && local_exit_code != 0) {
						log_msg(
						    label_exec_error_msg, hostname, host_labels[j]->name, local_exit_code);
						goto exit;
					}

					/* restore */
					if (restore_opt && host_labels[j]->export_paths[0])
						scp_exit_code =
						    scp_archive(hostname, socket_path, host_labels[j], http_port, true);

					if (stop_on_err_opt && scp_exit_code != 0) {
						log_msg(
						    label_exec_error_msg, hostname, host_labels[j]->name, scp_exit_code);
						goto exit;
					}

					/* remote execution */
					if (tty_opt)
						exit_code = ssh_command_tty(
						    hostname, socket_path, host_labels[j], http_port, env_override);
					else
						exit_code = ssh_command_pipe(
						    hostname, socket_path, host_labels[j], http_port, env_override);

					if (stop_on_err_opt && (exit_code != 0)) {
						log_msg(label_exec_error_msg, hostname, host_labels[j]->name, exit_code);
						goto exit;
					}

					/* archive */
					if (archive_opt && host_labels[j]->export_paths[0])
						scp_exit_code =
						    scp_archive(hostname, socket_path, host_labels[j], http_port, false);

					if (stop_on_err_opt && scp_exit_code != 0) {
						log_msg(
						    label_exec_error_msg, hostname, host_labels[j]->name, scp_exit_code);
						goto exit;
					}

					/* local end */
					local_exit_code = local_exec(host_labels[j], host_labels[j]->options.end);

					if (stop_on_err_opt && local_exit_code != 0) {
						log_msg(
						    label_exec_error_msg, hostname, host_labels[j]->name, local_exit_code);
						goto exit;
					}

					/* ssh terminated, unable to execute local interpreter */
					if ((exit_code == 255) || (exit_code == 127))
						log_msg(label_exec_error_msg, hostname, host_labels[j]->name, exit_code);
					else
						log_msg(label_exec_end_msg, hostname, host_labels[j]->name, exit_code);

					/* read output of web server */
					nr = read(stdout_pipe[0], httpd_log, sizeof(httpd_log));
					httpd_log[nr] = '\0';
					if (nr > 0)
						trace_http(httpd_log);
					if ((nr == -1) && (errno != EAGAIN))
						warn("read from httpd output");
				}

			exit:
				if (socket_path) {
					if (archive_opt || restore_opt)
						log_msg(host_disconnect_msg, hostname, "",
						    stop_on_err_opt ? exit_code : scp_exit_code);
					else
						log_msg(host_disconnect_msg, hostname, "",
						    stop_on_err_opt ? exit_code : local_exit_code);
					end_connection(socket_path, hostname, http_port);
					free(socket_path);
					socket_path = NULL;
				}
			}
		}
	}

	if (stop_on_err_opt)
		return exit_code || scp_exit_code;
	else
		return 0;
}

/*
 * Dry run: print hostnames and regex label matches
 * Returns an exit code
 */

static int
dry_run(char *hostnames[], Label **route_labels, regex_t *label_reg) {
	int i, j, k, l;
	regmatch_t regmatch;
	Label **host_labels;

	for (i = 0; route_labels[i]; i++) {
		host_labels = route_labels[i]->labels;

		for (k = 0; hostnames[k]; k++) {
			for (l = 0; l < route_labels[i]->n_aliases; l++) {
				if (strcmp(hostnames[k], route_labels[i]->aliases[l]) == 0)
					hostname = route_labels[i]->aliases[l];
				else
					continue;

				hl_range(hostname, HL_HOST, 0, strlen(hostname));
				printf("\n");
				for (j = 0; host_labels[j]; j++) {
					if (regexec(label_reg, host_labels[j]->name, 1, &regmatch, 0) != 0)
						continue;

					hl_range(host_labels[j]->name, HL_LABEL, regmatch.rm_so, regmatch.rm_eo);
					printf("\n");

					if (dryrun_opt)
						continue;
				}
			}
		}
	}

	return 0;
}

/* signal handlers */

void
handle_exit(int sig) {
	if (socket_path && hostname && http_port) {
		printf("caught signal %d, terminating connection to '%s'\n", sig, hostname);
		/* clean up socket and SSH connection; leaving staging dir */
		execlp("ssh", "ssh", "-S", socket_path, "-O", "exit", hostname, NULL);
		err(1, "ssh -O exit");
	}
}

/* internal utilty functions */

static void
usage() {
	fprintf(stderr, "release: %s\n", RELEASE);
	fprintf(stderr,
	    "usage: rset [-AenRtv] [-E environment] [-F sshconfig_file] [-f routes_file]\n"
	    "            [-x label_pattern] hostname ...\n"
	    "       rset [-ev] [-E environment] [-F sshconfig_file] [-f routes_file]\n"
	    "            [-x label_pattern] -o log_directory -p workers hostname ...\n");
	exit(1);
}

static void
set_options(int argc, char *argv[], char *hostnames[]) {
	int i;
	int ch;
	const char *errstr;
	opterr = 0;
	Options op;

	bzero(&op, sizeof op);

	while ((ch = getopt(argc, argv, "AenRtE:F:f:o:p:x:")) != -1) {
		switch (ch) {
		case 'A':
			archive_opt = 1;
			break;
		case 'e':
			stop_on_err_opt = 1;
			break;
		case 'n':
			dryrun_opt = 1;
			break;
		case 't':
			tty_opt = 1;
			break;
		case 'R':
			restore_opt = 1;
			break;
		case 'E':
			env_override = optarg;
			free(env_split_lines(env_override, env_override, 1));
			break;
		case 'F':
			sshconfig_file = optarg;
			break;
		case 'f':
			routes_file = optarg;
			break;
		case 'o':
			log_directory = optarg;
			break;
		case 'p':
			n_parallel = strtonum(optarg, 1, MAX_WORKERS, &errstr);
			if (errstr != NULL)
				errx(1, "number out of bounds %s: '%s'", errstr, argv[optind - 1]);
			break;
		case 'x':
			label_pattern = optarg;
			break;

		default:
			usage();
		}
	}
	if (optind >= argc)
		usage();

	if (n_parallel) {
		if (dryrun_opt || tty_opt || archive_opt || restore_opt)
			usage();
	}

	if ((log_directory == NULL) ^ (n_parallel == 0))
		usage();

	for (i = 0; i < argc - optind; i++)
		hostnames[i] = argv[optind + i];
	hostnames[i] = NULL;
}

static void
not_found(char *name) {
	errx(1, "'%s' not found in PATH", name);
}

/* ensure each hostname is found in the routes */

static void
compare_argv_routes(char *hostnames[], Label **route_labels) {
	int i, j, k;
	int labels_matched;

	for (i = 0; hostnames[i]; i++) {
		labels_matched = 0;
		for (j = 0; route_labels[j]; j++) {
			for (k = 0; k < route_labels[j]->n_aliases; k++) {
				if (strcmp(hostnames[i], route_labels[j]->aliases[k]) == 0)
					labels_matched++;
			}
		}
		if (labels_matched == 0)
			errx(1, "No match for '%s' in %s", hostnames[i], routes_file);
	}
}

/* built-in http server */

static void
start_http_server(int stdout_pipe[], int http_port) {
	int flags;
	int status;
	char *http_srv_argv[9], *inputstring;
	char *httpd_bin;
	pid_t http_server_pid;
	pid_t rset_pid;
	sigset_t set;

	/* Convert http server command line into a vector */
	inputstring = xmalloc(PATH_MAX, "inputstring");

	snprintf(inputstring, PATH_MAX, "miniquark -p %d -d " PUBLIC_DIRECTORY, http_port);
	str_to_array(http_srv_argv, inputstring, sizeof(http_srv_argv), " ");
	if ((httpd_bin = findprog(http_srv_argv[0])) == 0)
		not_found(http_srv_argv[0]);

	/* start the web server */
	xpipe(stdout_pipe, "stdout");
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
}
