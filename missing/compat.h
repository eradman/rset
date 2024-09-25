/* compat.h */

#if defined(_LINUX_PORT)
size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);
unsigned int arc4random(void);
long long strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);
#endif

#if defined(_MACOS_PORT) || defined(_LINUX_PORT)
void setproctitle(const char *fmt, ...);
#endif

#ifndef __OpenBSD__
#define pledge(s, p) (0)
#define unveil(s, p) (0)
#endif

#if defined(_MACOS_PORT)
#define st_mtim st_mtimespec
#endif
