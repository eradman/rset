/*
 * Copyright (c) 2018 Eric Radman <ericshane@eradman.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "execute.h"

int
get_socket() {
	int sock, port;
	socklen_t addrlen;
	struct sockaddr_in addr;

	addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(0);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	bind(sock, (struct sockaddr*) &addr, sizeof(addr));
	getsockname(sock, (struct sockaddr*) &addr, &addrlen);
	port = ntohs(addr.sin_port);
	close(sock);

	return port;
}

int
pipe_cmd(char *command, char *input, size_t len) {
	FILE *fd;

	fd = popen(command, "w");
	if (fd == NULL)
		err(1, "popen");
	fwrite(input, len, 1, fd);
	pclose(fd);

	return 0;
}

char*
ssh_command(char *host_name, char *socket_path, char *label_name, Options* options, int http_port) {
	char install_url[1024];
	char *cmd;
	Options op;

	if (strlen(options->install_url) > 0)
		strlcpy(install_url, options->install_url, sizeof(install_url));
	else
		strlcpy(install_url, DEFAULT_INSTALL_URL, sizeof(install_url));

	if (strlen(options->username) > 0)
		snprintf(op.ssh_options, sizeof(op.ssh_options), "%s -T -l %s",
		    options->ssh_options, options->username);
	else
		snprintf(op.ssh_options, sizeof(op.ssh_options), "%s -T",
		    options->ssh_options);

	if (strlen(options->interpreter) > 0)
		strlcpy(op.interpreter, options->interpreter, sizeof(op.interpreter));
	else
		strlcpy(op.interpreter, "/bin/sh", sizeof(op.interpreter));

	if (strlen(options->execute_with) > 0)
		strlcpy(op.execute_with, options->execute_with,
			sizeof(op.execute_with));
	else
		strlcpy(op.execute_with, "", sizeof(op.execute_with));

	cmd = malloc(PATH_MAX);
	snprintf(cmd, PATH_MAX, "ssh -S %s %s %s %s "
	    "'sh -c \"cd " REMOTE_TMP_PATH "; _=%s INSTALL_URL=%s exec %s\"'",
	    socket_path, op.ssh_options, host_name, op.execute_with,
	    http_port, label_name, install_url, op.interpreter);

	return cmd;
}

char *
start_connection(char *host_name, int http_port) {
	char cmd[PATH_MAX];
	char *socket_path;

	socket_path = malloc(128);
	snprintf(socket_path, 128, LOCAL_SOCKET_PATH, host_name);

	snprintf(cmd, PATH_MAX, "ssh -q -fnNT -R %d:localhost:%d -S %s -M %s",
	    DEFAULT_INSTALL_PORT, http_port, socket_path, host_name);
	if (system(cmd) != 0)
		err(1, "ssh -M");

	snprintf(cmd, PATH_MAX, "ssh -q -S %s %s mkdir " REMOTE_TMP_PATH,
	    socket_path, host_name, http_port);
	if (system(cmd) != 0)
		err(1, "mkdir failed");

	snprintf(cmd, PATH_MAX, "tar -cf - -C " REPLICATED_DIRECTORY " ./ | "
	   "ssh -q -S %s %s tar -xf - -C " REMOTE_TMP_PATH,
	    socket_path, host_name, http_port);
	if (system(cmd) != 0)
		err(1, "transfer failed for " REPLICATED_DIRECTORY);

	return socket_path;
}

void
end_connection(char *socket_path, char *host_name, int http_port) {
	char cmd[PATH_MAX];

	snprintf(cmd, PATH_MAX, "ssh -q -S %s %s rm -r " REMOTE_TMP_PATH,
	    socket_path, host_name, http_port);
	if (system(cmd) != 0)
		err(1, "remote tmp dir");

	snprintf(cmd, PATH_MAX, "ssh -q -S %s -O exit %s", socket_path, host_name);
	if (system(cmd) != 0)
		err(1, "ssh -O exit");

	unlink(socket_path);
	free(socket_path);
}
