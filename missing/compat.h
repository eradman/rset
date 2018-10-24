/* compat.h */

#if defined(_LINUX_PORT) && defined(__GLIBC__)
size_t strlcpy(char *to, const char *from, int l);
#endif

#if defined(_MACOS_PORT) || defined(_LINUX_PORT)
void setproctitle(const char *fmt, ...);
#endif

#ifndef __OpenBSD__
#define pledge(s, p) (0)
#define unveil(s, p) (0)
#endif
