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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "missing/compat.h"

#include "xlibc.h"

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
 * Mimic basename(3) on OpenBSD which does not modify it's input
 */
char *
xbasename(const char *path) {
	static char dname[PATH_MAX];

	strlcpy(dname, path, sizeof(dname));
	return basename(dname);
}

/*
 * Like stat(2) but retry until expected status
 */
int
xstat(const char *path, struct stat *sb, int expected) {
	int i;
	int ret;
	struct timespec delay = { 0, 1000000 };

	/* wait up to 0.5 seconds */
	for (i = 0; i < 5; i++) {
		ret = stat(path, sb);
		if (ret == expected)
			break;
		nanosleep(&delay, NULL);
	}
	return ret;
}

/*
 * Call strdup(3) and exit on failure
 */
void *
xstrdup(const char *s, const char *name) {
	void *p;
	p = strdup(s);
	if (p == NULL)
		err(1, "strdup > %s", name);
	return p;
}

/*
 * Call calloc(3) and exit on failure
 */
void *
xcalloc(size_t nmemb, size_t size, const char *name) {
	void *p;
	p = calloc(nmemb, size);
	if (p == NULL)
		err(1, "calloc > %s", name);
	return p;
}

/*
 * Call malloc(3) and exit on failure
 */
void *
xmalloc(size_t size, const char *name) {
	void *p;
	p = malloc(size);
	if (p == NULL)
		err(1, "malloc > %s", name);
	return p;
}

/*
 * Call realloc(3) and exit on failure or if request is over a limit
 */
void *
xrealloc(void *ptr, size_t size, const char *name) {
	void *p;
	if (size > REALLOC_MAX_SIZE)
		errx(1, "realloc > %s exceeds %d", name, REALLOC_MAX_SIZE);
	p = realloc(ptr, size);
	if (p == NULL)
		err(1, "realloc > %s", name);
	return p;
}

/*
 * Call pipe(2) and exit on failure
 */
int
xpipe(int *fildes, const char *name) {
	int ret;
	ret = pipe(fildes);
	if (ret == -1)
		err(1, "pipe < %s", name);
	return ret;
}
