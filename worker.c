/*
 * Copyright (c) 2024 Eric Radman <ericshane@eradman.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "worker.h"
#include "xlibc.h"

/*
 * create_worker_argv - assemble argv for workers
 * execute_worker - fork background process and direct stdout/stderr to logfile
 * run_status - execute status script in a loop until children terminate
 */

int
create_worker_argv(char *argv[], char *worker_argv[]) {
	int argc;
	int skip;

	for (argc = 0, skip = 0; argc < optind; argc++) {
		if (argv[argc][0] == '-') {
			switch (argv[argc][1]) {
			case 'o':
			case 'p':
				if (argv[argc][2] == '\0') {
					argc++;
					skip++;
				}
				skip++;
				continue;
			}
		}
		worker_argv[argc - skip] = argv[argc];
	}
	worker_argv[argc - skip] = NULL;
	return argc - skip;
}

int
exec_worker(char *log_directory, int worker_id, char *worker_argv[]) {
	int logfd;
	pid_t rset_pid;

	rset_pid = fork();
	if (rset_pid == -1)
		err(1, "fork worker");
	if (rset_pid == 0) {
		logfd = open_log(log_directory, worker_id);

		if (dup2(logfd, fileno(stdout)) == -1)
			err(255, "redirect stdout");
		if (dup2(logfd, fileno(stderr)) == -1)
			err(255, "redirect stderr");

		setenv("RSET_HOST_CONNECT", "%s|%T|HOST_CONNECT|%h|", 1);
		setenv("RSET_HOST_CONNECT_ERROR", "%s|%T|HOST_CONNECT_ERROR|%h|%e", 1);
		setenv("RSET_LABEL_EXEC_BEGIN", "%s|%T|EXEC_BEGIN|%l|", 1);
		setenv("RSET_LABEL_EXEC_END", "%s|%T|EXEC_END|%l|%e", 1);
		setenv("RSET_LABEL_EXEC_ERROR", "%s|%T|EXEC_ERROR|%l|%e", 1);
		setenv("RSET_HOST_DISCONNECT", "%s|%T|HOST_DISCONNECT|%h|%e", 1);
		unsetenv("HTTP_TRACE");
		unsetenv("SSH_TRACE");

		execvp(worker_argv[0], worker_argv);
		err(1, "Failed to start worker '%s'", worker_argv[0]);
	}

	return rset_pid;
}

void
rexec_summary(int n_workers, int worker_pid[], char *log_directory) {
	int i;
	int status_argc;
	int status;
	int remaining;
	char *status_argv[MAX_WORKERS];
	pid_t pid, status_pid;

	const struct timespec timeout = { 0, 500000000 }; /* 0.5s */

	status_argv[0] = "rexec-summary";
	for (i = 1; i <= n_workers; i++)
		asprintf(&status_argv[i], "%s/%s.%d", log_directory, get_tmstr(), i);
	status_argc = i;
	status_argv[status_argc] = NULL;

	remaining = n_workers;
	while (remaining > 0) {
		nanosleep(&timeout, NULL);
		for (i = 0; i < n_workers; i++) {
			if (worker_pid[i]) {
				pid = waitpid(worker_pid[i], &status, WNOHANG);
				if (pid == -1)
					warn("wait for pid %d", worker_pid[i]);
				else if (pid == worker_pid[i]) {
					worker_pid[i] = 0;
					remaining--;
				}
			}
		}

		if (remaining < 1) {
			status_argv[status_argc++] = "/dev/null";
			status_argv[status_argc] = NULL;
		}

		status_pid = fork();
		if (status_pid == -1)
			err(1, "fork status");
		if (status_pid == 0) {
			execvp(status_argv[0], status_argv);
			err(1, "exec %s", status_argv[0]);
		}
		waitpid(status_pid, &status, 0);
	}
}

/*
 * get_tmstr - timestamp use for all worker log files
 */
char *
get_tmstr() {
	static struct tm *tm;
	static time_t tv = 0;
	static char log_tmstr[64];

	if (tv == 0) {
		tv = time(NULL);
		tm = localtime(&tv);
		strftime(log_tmstr, sizeof log_tmstr, WORKER_TIMESTAMP_FORMAT, tm);
	}
	return log_tmstr;
}

/*
 * open_log - create and open log file for worker output
 */
int
open_log(char *log_directory, int worker_id) {
	int logfd;
	char logfn[PATH_MAX];

	snprintf(logfn, sizeof logfn, "%s/%s.%d", log_directory, get_tmstr(), worker_id);
	logfd = open(logfn, O_RDWR | O_CREAT, 0640);
	if (logfd == -1)
		err(1, "open %s", logfn);

	return logfd;
}
