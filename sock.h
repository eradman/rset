/* miniquark: see LICENSE file for copyright and license details. */

int sock_get_ips(const char *, const char *);
int sock_set_timeout(int, int);
int sock_get_inaddr_str(const struct sockaddr_storage *, char *, size_t);
