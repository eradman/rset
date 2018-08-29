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
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"
#include "config.h"
#include "rutils.h"

/*
 * Mimic dirname(3) on OpenBSD which does not modify it's input
 */
char *
xdirname(const char *path) {
	static char dname[PATH_MAX];

	strlcpy(dname, path, sizeof(dname));
	return dirname(dname);
}

/*
 * install_if_new - install a file and parent directory if it does not already exist
 */
void
install_if_new(const char *src, const char *dst) {
	int pid;
	int status;
	mode_t dir_mode;
	struct stat src_sb;
	struct stat dst_sb;

	if (stat(src, &src_sb) == -1)
		err(1, "%s", src);

	if (stat(dst, &dst_sb) == -1) {
		printf("rset: initialized directory '%s'\n", xdirname(dst));
		dir_mode = 0750;
		(void) mkdir(xdirname(dst), dir_mode);
	}
	else {
		if (src_sb.st_mtime > dst_sb.st_mtime)
			printf("rset: updating '%s'\n", dst);
		else
			return;
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
		match = strndup(s+so, eo-so);
		printf("%s%s" HL_REVERSE "%s" HL_RESET "%s%s"
		    HL_RESET, color, start, match, color, s+eo);
		free(start); free(match);
	}
}

/*
 * format_options - provide a concise representation of options
 */

char *
format_options(Options *op) {
	static char buf[2048];
	int pos = 0;

	if (*op->interpreter)
		pos += snprintf(buf+pos, sizeof(buf)-pos, "interpreter=%s,", op->interpreter);
	if (*op->execute_with)
		pos += snprintf(buf+pos, sizeof(buf)-pos, "execute_with=%s,", op->execute_with);
	if (*op->install_url)
		pos += snprintf(buf+pos, sizeof(buf)-pos, "install_url=%s,", op->install_url);
	buf[pos-1] = '\0';

	return buf;
}
