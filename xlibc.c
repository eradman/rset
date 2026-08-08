/*
 * xlibc.c
 * Standard libc function augmented with behavior expected by rset
 */

#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "missing/compat.h"

#include "rutils.h"
#include "xlibc.h"

/*
 * Mimic dirname(3) on OpenBSD which does not modify it's input
 */
char *
xdirname(const char *path) {
	static char dname[PATH_MAX];

	str_cpy(dname, path, sizeof(dname));
	return dirname(dname);
}

/*
 * Mimic basename(3) on OpenBSD which does not modify it's input
 */
char *
xbasename(const char *path) {
	static char dname[PATH_MAX];

	str_cpy(dname, path, sizeof(dname));
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

/*
 * Call regcomp(3) and format expression errors
 */
int
xregcomp(regex_t *preg, const char *pattern, int cflags) {
	int rv;
	char buf[100];
	if ((rv = regcomp(preg, pattern, cflags)) != 0) {
		regerror(rv, preg, buf, sizeof(buf));
		errx(1, "bad expression: %s", buf);
	}
	return rv;
}

/*
 * Call regexec(3) and refuse zero-width matches
 */
int
xregexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[]) {
	int rv;
	int eflags = 0;
	rv = regexec(preg, string, nmatch, pmatch, eflags);
	if (pmatch->rm_so == pmatch->rm_eo)
		rv = REG_NOMATCH;
	return rv;
}
