/* compat.h */

#if defined(_LINUX_PORT) && defined(__GLIBC__)
size_t strlcpy(char *to, const char *from, int l);
#endif

#if defined(_MACOS_PORT) || defined(_LINUX_PORT)
void setproctitle(const char *fmt, ...);
long long strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);
#endif

#ifndef __OpenBSD__
#define pledge(s, p) (0)
#define unveil(s, p) (0)
#endif

#if defined(_MACOS_PORT)
#define st_mtim st_mtimespec
#endif
