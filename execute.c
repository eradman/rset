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
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <paths.h>
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
 * findprog - from which(1)
 */

char *
findprog(char *prog)
{
	int len;
	char *path;
	char *filename;
	char *p, *pathcpy;
	struct stat sbuf;

	path = getenv("PATH");
	if ((path = strdup(path)) == NULL)
		err(1, "strdup");
	pathcpy = path;
	filename = malloc(PATH_MAX);

	while ((p = strsep(&pathcpy, ":")) != NULL) {
		if (*p == '\0')
			p = ".";

		len = strlen(p);
		while (len > 0 && p[len-1] == '/')
			p[--len] = '\0';  /* strip trailing '/' */

		(void) snprintf(filename, PATH_MAX, "%s/%s", p, prog);
		if ((stat(filename, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(filename, X_OK) == 0) {
			(void) free(path);
			return filename;
		}
	}
	(void) free(path);
	(void) free(filename);
	return NULL;
}

/*
 *  start_connection - start an SSH control master and copy _rutils
 *  ssh_command_pipe - execute a script over a pipe to a remote interpreter
 *  ssh_command_tty  - copy script to remote host before execution
 *  end_connection   - stop an SSH control master and remove temporary files
 */

char *
start_connection(Label *route_label, int http_port, const char *ssh_config) {
	int argc;
	char cmd[PATH_MAX];
	char tmp_path[64];
	char port_forwarding[64];
	char *socket_path;
	char *argv[32];
	char *host_name;
	char **path;
	struct stat sb;

	/* verify that export paths are accessible */
	path = route_label->export_paths;
	while (path && *path) {
		if (stat(*path, &sb) == -1)
			err(1, "%s: unable to stat '%s'", route_label->name, *path);
		path++;
	}

	/* construct command to execute on remote host  */
	host_name = route_label->name;

	socket_path = malloc(128);
	snprintf(socket_path, 128, LOCAL_SOCKET_PATH, host_name);

	snprintf(port_forwarding, 64, "%d:localhost:%d", INSTALL_PORT, http_port);

	if (stat(socket_path, &sb) != -1) {
		fprintf(stderr, "rset: socket for '%s' already exists, run\n"
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

	snprintf(cmd, PATH_MAX, "tar -cf - %s -C " REPLICATED_DIRECTORY " ./ | "
	   "exec ssh -q -S %s %s tar -xf - -C " REMOTE_TMP_PATH,
	    array_to_str(route_label->export_paths), socket_path, host_name,
	    http_port);
	if (system(cmd) != 0)
		err(1, "transfer failed for " REPLICATED_DIRECTORY);

	return socket_path;
}

int
ssh_command_pipe(char *host_name, char *socket_path, Label *host_label, int http_port) {
	int argc;
	char cmd[PATH_MAX];
	char *argv[32];
	Options op;

	/* construct command to execute on remote host  */
	apply_default(op.interpreter, host_label->options.interpreter, INTERPRETER);
	apply_default(op.execute_with, host_label->options.execute_with, EXECUTE_WITH);

	snprintf(cmd, sizeof(cmd), "%s sh -c \"cd " REMOTE_TMP_PATH "; LABEL='%s' "
	    "ROUTE_LABEL='%s' INSTALL_URL='" INSTALL_URL "' exec %s\"",
	    op.execute_with, http_port, host_label->name, host_name,
	    op.interpreter);

	/* construct ssh command */
	argc = 0;
	argc = append(argv, argc, "ssh", "-T", "-S", socket_path, NULL);

	(void) append(argv, argc, host_name, cmd, NULL);
	return pipe_cmd(argv, host_label->content, host_label->content_size);
}

int
ssh_command_tty(char *host_name, char *socket_path, Label *host_label, int http_port) {
	int argc;
	char cmd[PATH_MAX];
	char *argv[32];
	Options op;

	/* copy the contents of the script */
	snprintf(cmd, sizeof(cmd), "cat > " REMOTE_SCRIPT_PATH,
	    http_port);
	/* construct ssh command */
	argc = 0;
	argc = append(argv, argc, "ssh", "-T", "-S", socket_path, NULL);
	(void) append(argv, argc, host_name, cmd, NULL);
	pipe_cmd(argv, host_label->content, host_label->content_size);

	/* construct command to execute on remote host  */
	apply_default(op.interpreter, host_label->options.interpreter, INTERPRETER);
	apply_default(op.execute_with, host_label->options.execute_with, EXECUTE_WITH);

	snprintf(cmd, sizeof(cmd), "%s sh -c \"cd " REMOTE_TMP_PATH "; LABEL='%s' "
	    "ROUTE_LABEL='%s' INSTALL_URL='" INSTALL_URL "' exec %s "
	    REMOTE_SCRIPT_PATH "\"",
	    op.execute_with, http_port, host_label->name, host_name,
	    op.interpreter, http_port);

	/* construct ssh command */
	argc = 0;
	argc = append(argv, argc, "ssh", "-t", "-S", socket_path, NULL);

	(void) append(argv, argc, host_name, cmd, NULL);
	return run(argv);
}

void
end_connection(char *socket_path, char *host_name, int http_port) {
	char tmp_path[64];
	char *argv[32];

	snprintf(tmp_path, sizeof(tmp_path), REMOTE_TMP_PATH, http_port);
	append(argv, 0, "ssh", "-S", socket_path, host_name, "rm", "-rf", tmp_path , NULL);
	if (run(argv) != 0)
		warn("remote tmp dir");

	append(argv, 0, "ssh", "-q", "-S", socket_path, "-O", "exit", host_name, NULL);
	if (run(argv) != 0)
		err(1, "exec ssh -O exit");

	free(socket_path);
}

/* internal utility functions */

void
apply_default(char *option, const char *user_option, const char *default_option) {
	if (strlen(user_option) > 0)
		memcpy(option, user_option, strlen(user_option)+1);
	else
		memcpy(option, default_option, strlen(default_option)+1);
}

