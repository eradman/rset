/* compat.h */

#if defined(_MACOS_PORT)
void setproctitle(const char *fmt, ...);
#endif
