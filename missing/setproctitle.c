/*
 * setproctitle.c
 * No-op on Linux
 */

void
setproctitle(const char *fmt, ...) {
	return;
}
