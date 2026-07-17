/*
 * rutils.c
 * Utility functions for rset
 */

#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <regex.h>
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
 * str_cpy - copy string, format error and abort on overflow
 */
size_t
str_cpy(char *dst, const char *src, size_t dsize) {
	char err_str[20];
	int i;
	size_t len;

	len = strlen(src);
	if (len + 1 > dsize) {
		for (i = 0; i < len && i < sizeof(err_str) - 1; i++) {
			err_str[i] = *(src + i);
			/* erase control characters */
			if ((int) err_str[i] < 32)
				err_str[i] = ' ';
		}
		err_str[i] = '\0';
		errx(1, "input too long: %s.. > %zu bytes", err_str, dsize);
	}

	memcpy(dst, src, len + 1);
	return len;
}

/*
 * str_to_array - split a string using the input string as the buffer
 * array_to_str - format an array using the output string as the buffer
 * array_append - add a list of arguments to an array
 */
int
str_to_array(char *argv[], const char *inputstring, int max_elements, const char *delim) {
	int argc;
	char *input;
	char **ap;

	input = xstrdup(inputstring, "input");

	argc = 0;
	for (ap = argv; ap < &argv[max_elements] && (*ap = strsep(&input, delim)) != NULL;) {
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
		argc += str_cpy(p + argc, *argv, max_length - argc);
		argv++;
		if (argv && *argv)
			argc += str_cpy(p + argc, delim, max_length - argc);
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
 * pattern_match - match exact string or regular expression
 */
const char *
pattern_match(const char *pattern, const char *string) {
	char buf[_POSIX2_LINE_MAX];
	char c;
	int n, len;
	int rv;
	bool is_char, is_digit, is_dash, is_dot, is_colon;
	bool try_regex = false;
	const char *match = NULL;
	regex_t label_reg;
	regmatch_t regmatch;

	/* test for input consistant with a valid hostname or address */
	len = strlen(pattern);
	for (n = 0; n < len; n++) {
		c = pattern[n];
		is_char = (c >= 'a') && (c <= 'z');
		is_digit = (c >= '0') && (c <= '9');
		is_dash = (c == '-') && (n > 0) && (n + 1 < len);
		is_dot = (c == '.') && (n > 0) && (n + 1 < len);
		is_colon = (c == ':') && (n + 1 < len);
		if (is_char | is_digit | is_dash | is_dot | is_colon)
			continue;
		else
			try_regex = true;
	}

	if (try_regex) {
		if ((rv = regcomp(&label_reg, pattern, REG_EXTENDED)) != 0) {
			regerror(rv, &label_reg, buf, sizeof(buf));
			errx(1, "bad expression: %s", buf);
		}
		if (regexec(&label_reg, string, 1, &regmatch, 0) == 0) {
			if (regmatch.rm_eo > regmatch.rm_so)
				match = string;
		}
	} else {
		if (strcmp(pattern, string) == 0)
			match = string;
	}
	return match;
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
				index += str_cpy(buf + index, hostname, sizeof(buf) - index);
				break;
			case 'l':
				index += str_cpy(buf + index, label_name, sizeof(buf) - index);
				break;
			case 's':
				index += snprintf(buf + index, sizeof(buf) - index, "%08" PRIx32, session_id);
				break;
			case 'T':
				strftime(tmstr, sizeof(tmstr), LOG_TIMESTAMP_FORMAT, tm);
				index += str_cpy(buf + index, tmstr, sizeof(buf) - index);
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

	bp = input = xstrdup(http_log, "http_log");

	while ((ap = strsep(&input, "\n")) != NULL) {
		if (*ap != '\0')
			printf("+ " HL_TRACE "%s" HL_RESET "\n", ap);
	}
	free(bp);
}
