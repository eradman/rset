/*
 * xlibc.c
 * Standard libc function augmented with behavior expected by rset
 */

#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "missing/compat.h"

#include "rutils.h"
#include "xlibc.h"

/*
 * xdirname - mimic dirname(3) on OpenBSD which does not modify input
 * xbasename - mimic basename(3) on OpenBSD which does not modify input
 * xchdir - call chdir(2) and exit on failure
 */
char *
xdirname(const char *path) {
	static char dname[PATH_MAX];

	str_cpy(dname, path, sizeof(dname));
	return dirname(dname);
}

char *
xbasename(const char *path) {
	static char dname[PATH_MAX];

	str_cpy(dname, path, sizeof(dname));
	return basename(dname);
}

int
xchdir(const char *path) {
	int rv;
	rv = chdir(path);
	if (rv == -1)
		err(1, "chdir %s", path);
	return rv;
}

/*
 * xstat - call stat(2) and retry until expected status
 * xpipe - call pipe(2) and exit on failure
 * xwrite - call write(2) and exit on failure
 */
int
xstat(const char *path, struct stat *sb, int expected) {
	int rv;
	int i;
	struct timespec delay = { 0, 1000000 };

	/* wait up to 0.5 seconds */
	for (i = 0; i < 5; i++) {
		rv = stat(path, sb);
		if (rv == expected)
			break;
		nanosleep(&delay, NULL);
	}
	return rv;
}

int
xpipe(int *fildes, const char *var_name) {
	int rv;
	rv = pipe(fildes);
	if (rv == -1)
		err(1, "pipe < %s", var_name);
	return rv;
}

size_t
xwrite(int fd, const void *buf, size_t nbytes, const char *var_name) {
	size_t rv;
	rv = write(fd, buf, nbytes);
	if (rv == -1)
		errx(1, "write > %s", var_name);
	return rv;
}

/*
 * xstrdup - call strdup(3) and exit on failure
 * xmalloc - call malloc(3) and exit on failure
 * xrealloc - call realloc(3) and exit on failure or if request limit is exceeded
 */
void *
xstrdup(const char *s, const char *var_name) {
	void *p;
	p = strdup(s);
	if (p == NULL)
		err(1, "strdup > %s", var_name);
	return p;
}

void *
xmalloc(size_t size, const char *var_name) {
	void *p;
	p = malloc(size);
	if (p == NULL)
		err(1, "malloc > %s", var_name);
	return p;
}

void *
xrealloc(void *ptr, size_t size, const char *var_name) {
	void *p;
	if (size > REALLOC_MAX_SIZE)
		errx(1, "realloc > %s exceeds %d", var_name, REALLOC_MAX_SIZE);
	p = realloc(ptr, size);
	if (p == NULL)
		err(1, "realloc > %s", var_name);
	return p;
}

/*
 * xregcomp - call regcomp(3) and format expression errors
 * xregexec - call regexec(3) and refuse zero-width matches
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

int
xregexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[]) {
	int rv;
	int eflags = 0;
	rv = regexec(preg, string, nmatch, pmatch, eflags);
	if (pmatch->rm_so == pmatch->rm_eo)
		rv = REG_NOMATCH;
	return rv;
}

/*
 * xsnprintf - call snprintf(3) and assert that string was not truncated
 * xasprintf - call asprintf(3) and exit on failure
 */
int
xsnprintf(char *str, size_t size, const char *format, ...) {
	int rv;
	va_list va;
	va_start(va, format);
	rv = vsnprintf(str, size, format, va);
	va_end(va);

	if (rv < 0)
		err(1, "snprintf > '%s'", format);
	if (rv > size)
		errx(1, "formatted input too long: '%s' > %zu bytes", format, size);
	return rv;
}

int
xasprintf(char **ret, const char *format, ...) {
	int rv;
	va_list va;
	va_start(va, format);
	rv = vasprintf(ret, format, va);
	va_end(va);

	if (rv < 0)
		err(1, "asprintf > '%s'", format);
	return rv;
}
