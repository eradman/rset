/*
 * xlibc.c
 * Standard libc function augmented with behavior expected by rset
 */

#include <regex.h>

#include <sys/stat.h>

#define REALLOC_MAX_SIZE 1048576

/* forwards */

char *xdirname(const char *);
char *xbasename(const char *);
int xstat(const char *, struct stat *, int);
void *xstrdup(const char *, const char *);
void *xcalloc(size_t, size_t, const char *);
void *xmalloc(size_t, const char *);
void *xrealloc(void *, size_t, const char *);
int xpipe(int *, const char *);
int xregcomp(regex_t *, const char *, int);
int xregexec(const regex_t *, const char *, size_t, regmatch_t[]);
