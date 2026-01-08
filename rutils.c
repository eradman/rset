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

#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "missing/compat.h"

#include "config.h"
#include "rutils.h"
#include "xlibc.h"

unsigned session_id;

/* globals */

int dir_mode = 0700;

/*
 * str_to_array - split a string using the input string as the buffer
 * array_to_str - format an array using the output string as the buffer
 * array_append - add a list of arguments to an array
 */
int
str_to_array(char *argv[], char *inputstring, int max_elements, const char *delim) {
	int argc;
	char **ap;

	argc = 0;
	for (ap = argv; ap < &argv[max_elements] && (*ap = strsep(&inputstring, delim)) != NULL;) {
		argc++;
		if (**ap != '\0')
			ap++;
	}
	*ap = NULL;
	return argc;
}

int
array_to_str(char *argv[], char *outputstring, int max_length, const char *delim) {
	int argc = 0;
	char *p = outputstring;

	while (argv && *argv) {
		argc += strlcpy(p + argc, *argv, max_length - argc);
		argv++;
		if (argv && *argv)
			argc += strlcpy(p + argc, delim, max_length - argc);
	}
	outputstring[argc] = '\0';
	return argc;
}

int
array_append(char *argv[], int argc, char *arg1, ...) {
	char *s;
	va_list ap;

	va_start(ap, arg1);
	for (s = arg1; s != NULL; s = va_arg(ap, char *))
		argv[argc++] = s;
	va_end(ap);
	argv[argc] = NULL;
	return argc;
}

/*
 * Update global session ID before starting a new SSH session
 */
unsigned
generate_session_id() {
	while ((session_id = arc4random()) == 0)
		;
	return session_id;
}

unsigned
current_session_id() {
	return session_id;
}

/*
 * check_permissions - verify and memorize top-level directory permissions
 */

void
check_permissions(const char *dir) {
	struct stat stat_buf;

	stat(dir, &stat_buf);
	if (stat_buf.st_mode & (S_IWGRP | S_IRWXO))
		errx(1, "invalid permissions for %s: mode must be u=rwx (0700) or u=rwx,g=rx (0750)", dir);
	dir_mode = stat_buf.st_mode;
}

/*
 * create_dir - ensure a directory exists
 * install_if_new - ensure a file is up to date
 */
int
create_dir(const char *dir) {
	struct stat dst_sb;

	if (stat(dir, &dst_sb) == -1) {
		printf("rset: initialized directory '%s'\n", dir);
		(void) mkdir(dir, dir_mode);
		return 1;
	}
	return 0;
}

void
install_if_new(const char *src, const char *dst) {
	int pid;
	int status;
	struct stat src_sb;
	struct stat dst_sb;

	if (stat(src, &src_sb) == -1)
		err(1, "%s", src);

	if (create_dir(xdirname(dst)) == 0) {
		if ((stat(dst, &dst_sb) == -1) || (src_sb.st_mtime > dst_sb.st_mtime))
			printf("rset: updating '%s'\n", dst);
		else
			return; /* directory exists and no update required */
	}

	pid = fork();
	if (pid == 0) {
		if (execl("/usr/bin/install", "/usr/bin/install", src, dst, NULL) != -1)
			err(1, "%s", dst);
	}
	waitpid(pid, &status, 0);
	if (status != 0)
		warnx("copy failed %s -> %s", src, dst);
}

/*
 * hl_range - colorize a line, reversing parts that match a range
 */
void
hl_range(const char *s, const char *color, unsigned so, unsigned eo) {
	char *start, *match;

	if (so == 0 && eo == 0)
		printf("%s%s" HL_RESET, color, s);
	else {
		start = strndup(s, so);
		match = strndup(s + so, eo - so);
		printf("%s%s" HL_REVERSE "%s" HL_RESET "%s%s" HL_RESET, color, start, match, color, s + eo);
		free(start);
		free(match);
	}
}

/*
 * log_msg - write log message and interpolate variables
 */

void
log_msg(char *template, char *hostname, char *label_name, int exit_code) {
	char *p;
	char buf[_POSIX2_LINE_MAX];
	char tmstr[64];
	int index = 0;
	time_t tv;
	struct tm *tm;

	p = template;
	tv = time(NULL);
	tm = localtime(&tv);

	if (!template)
		return;

	while (p[0] != '\0') {
		if (p[0] == '%') {
			p++;
			switch (p[0]) {
			case 'e':
				index += snprintf(buf + index, sizeof(buf) - index, "%d", exit_code);
				break;
			case 'h':
				index += strlcpy(buf + index, hostname, sizeof(buf) - index);
				break;
			case 'l':
				index += strlcpy(buf + index, label_name, sizeof(buf) - index);
				break;
			case 's':
				index += snprintf(buf + index, sizeof(buf) - index, "%08" PRIx32, session_id);
				break;
			case 'T':
				strftime(tmstr, sizeof(tmstr), LOG_TIMESTAMP_FORMAT, tm);
				index += strlcpy(buf + index, tmstr, sizeof(buf) - index);
				break;
			case '%':
				buf[index++] = p[0];
				break;
			default:
				break;
			}
		} else
			buf[index++] = p[0];
		p++;
	}
	buf[index++] = '\n';
	write(STDOUT_FILENO, buf, index);
}

/*
 * trace_shell - log ssh commands using system(3)
 * trace_exec  - log ssh commands using execvp(3)
 * trace_http  - format log messages emitted by miniquark(1)
 */
void
trace_shell(char *cmd) {
	if (!getenv("SSH_TRACE"))
		return;

	printf("+ " HL_TRACE "sh -c \"%s\"" HL_RESET "\n", cmd);
}

void
trace_exec(char *cmd[]) {
	char argv_repr[PATH_MAX];

	if (!getenv("SSH_TRACE"))
		return;

	array_to_str(cmd, argv_repr, sizeof(argv_repr), " ");
	printf("+ " HL_TRACE "%s" HL_RESET "\n", argv_repr);
}

void
trace_http(const char *http_log) {
	char *ap, *bp, *input;

	if (!getenv("HTTP_TRACE"))
		return;

	bp = input = strdup(http_log);
	if (input == NULL)
		err(1, "strdup");

	while ((ap = strsep(&input, "\n")) != NULL) {
		if (*ap != '\0')
			printf("+ " HL_TRACE "%s" HL_RESET "\n", ap);
	}
	free(bp);
}
