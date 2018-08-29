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
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"
#include "config.h"
#include "execute.h"

/*
 * append - add a list of arguments to an array
 */
int
append(char *argv[], int argc, char *arg1, ...) {
	char *s;
	va_list ap;

	va_start(ap, arg1);
	for (s = arg1; s != NULL; s = va_arg(ap, char *))
		argv[argc++] = s;
	va_end(ap);
	argv[argc] = NULL;
	return argc;
}

/*
 * run - standard function for running a subprocess
 */
int
run(char *const argv[]) {
	int status;
	pid_t pid;
	
	pid = fork();
	switch (pid) {
		case -1:
			err(1, "fork");
		case 0:
			execvp(argv[0], argv);
			err(1, "%s", argv[0]);
	}
	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid on %d", pid);
	return WEXITSTATUS(status);
}

/*
 * pipe_cmd - attach an input string to stdin and execute a utility
 */
int
pipe_cmd(char *const argv[], char *input, size_t len) {
	int status;
	int fd[2];
	pid_t pid;

	pipe(fd);
	pid = fork();
	if (pid == -1)
		err(1, "fork");

	if (pid == 0) {
		close(fd[1]);
		dup2(fd[0], STDIN_FILENO);
		execvp(argv[0], argv);
		err(1, "could not exec %s", argv[0]);
	}
	close(fd[0]);
	if (write(fd[1], input, len) == -1)
		err(1, "write to child");
	close(fd[1]);
	if (waitpid(pid, &status, 0) == -1)
		err(1, "wait on pid %d", pid);

	return status;
}

/*
 * get_socket - return an unused TCP port
 */
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

/*
 *  SSH functions
 *
 *  ssh_command      - main function for executing a script on a remote host
 *  start_connection - start an SSH control master and copy _rutils
 *  end_connection   - stop an SSH control master and remove temporary files
 */
int
ssh_command(char *host_name, char *socket_path, Label *host_label, int http_port) {
	int argc;
	char cmd[PATH_MAX];
	char *argv[32];
	Options op;

	/* construct command to execute on remote host  */

	apply_default(op.install_url, host_label->options.install_url, INSTALL_URL);
	apply_default(op.interpreter, host_label->options.interpreter, INTERPRETER);
	apply_default(op.execute_with, host_label->options.execute_with, EXECUTE_WITH);

	snprintf(cmd, sizeof(cmd), "%s sh -c \"cd " REMOTE_TMP_PATH "; LABEL='%s' INSTALL_URL='%s' exec %s\"",
	    op.execute_with, http_port, host_label->name, op.install_url, op.interpreter);

	/* construct ssh command */

	argc = 0;
	argc = append(argv, argc, "ssh", "-T", "-S", socket_path, NULL);

	(void)append(argv, argc, host_name, cmd, NULL);
	return pipe_cmd(argv, host_label->content, host_label->content_size);
}

char *
start_connection(char *host_name, int http_port, const char *ssh_config) {
	int argc;
	char cmd[PATH_MAX];
	char tmp_path[64];
	char port_forwarding[64];
	char *socket_path;
	char *argv[32];
	struct stat sb;

	socket_path = malloc(128);
	snprintf(socket_path, 128, LOCAL_SOCKET_PATH, host_name);

	snprintf(port_forwarding, 64, "%d:localhost:%d", INSTALL_PORT, http_port);

	if (stat(socket_path, &sb) != -1) {
		fprintf(stderr, "Error: socket for '%s' already exists, run\n"
		    "  fstat %s\n"
		    "and remove the file if no process is listed.\n",
		    host_name, socket_path);
		free(socket_path);
		return NULL;
	}

	argc = 0;
	argc = append(argv, argc, "ssh", "-fN", "-R", port_forwarding, "-S",
		socket_path, "-M", NULL);
	if (ssh_config)
		(void) append(argv, argc, "-F", ssh_config, host_name, NULL);
	else
		(void) append(argv, argc, host_name, NULL);
	if (run(argv) == 255) {
		free(socket_path);
		return NULL;
	}

	snprintf(tmp_path, sizeof(tmp_path), "mkdir " REMOTE_TMP_PATH, http_port);
	append(argv, 0, "ssh", "-S", socket_path, host_name, tmp_path, NULL);
	if (run(argv) != 0)
		err(1, "mkdir failed");

	snprintf(cmd, PATH_MAX, "tar -cf - -C " REPLICATED_DIRECTORY " ./ | "
	   "exec ssh -q -S %s %s tar -xf - -C " REMOTE_TMP_PATH,
	    socket_path, host_name, http_port);
	if (system(cmd) != 0)
		err(1, "transfer failed for " REPLICATED_DIRECTORY);

	return socket_path;
}

void
end_connection(char *socket_path, char *host_name, int http_port) {
	char tmp_path[64];
	char *argv[32];

	snprintf(tmp_path, sizeof(tmp_path), REMOTE_TMP_PATH, http_port);
	append(argv, 0, "ssh", "-S", socket_path, host_name, "rm", "-r", tmp_path , NULL);
	if (run(argv) != 0)
		err(1, "remote tmp dir");

	append(argv, 0, "ssh", "-q", "-S", socket_path, "-O", "exit", host_name, NULL);
	if (run(argv) != 0)
		err(1, "exec ssh -O exit");

	free(socket_path);
}

/* internal utility functions */

static void
apply_default(char *option, const char *user_option, const char *default_option) {
	if (strlen(user_option) > 0)
		memcpy(option, user_option, strlen(user_option)+1);
	else
		memcpy(option, default_option, strlen(default_option)+1);
}

