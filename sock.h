/* miniquark: see LICENSE file for copyright and license details. */

#define LISTEN_MAX 4

#include <poll.h>

int addr_listen(const char *, const char *, struct pollfd *);
int sock_set_timeout(int, int);
int sock_get_inaddr_str(const struct sockaddr_storage *, char *, size_t);
