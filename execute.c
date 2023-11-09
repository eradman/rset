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

#define BLOCK_SIZE 512

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
 * cmd_pipe_stdout - run a command, set exit_code and capture/return stdout
 */

char *
cmd_pipe_stdout(char *const argv[], int *error_code, int *output_size) {
	int nr, nbytes;
	int buffer_size;
	int status;
	int stdout_pipe[2];
	char buf[BLOCK_SIZE];
	char *output;
	char *newp;
	pid_t pid;

	nbytes = 0;
	buffer_size = ALLOCATION_SIZE;
	output = malloc(buffer_size + 1);  /* Add room for NULL */
	*error_code = -1;

	pipe(stdout_pipe);
	pid = fork();
	if (pid == -1)
		err(1, "fork");

	if (pid == 0) {
		/* child closes the output side */
		close(stdout_pipe[0]);

		dup2(stdout_pipe[1], STDOUT_FILENO);
		execvp(argv[0], argv);
		err(1, "could not exec %s", argv[0]);
	}

	/* parent closes the output side */
	close(stdout_pipe[1]);

	while ((nr = read(stdout_pipe[0], buf, BLOCK_SIZE)) != -1 && nr != 0) {
		/* ensure we have enough space to terminate string */
		if (nbytes + nr + 1 > buffer_size) {
			buffer_size += ALLOCATION_SIZE;
			if ((newp = realloc(output, buffer_size + 1)) == NULL)
				err(1, "realloc");
			output = newp;
		}
		memcpy(output + nbytes, buf, nr);
		nbytes += nr;
	}

	*(output + nbytes) = '\0';

	if (waitpid(pid, &status, 0) == -1)
		err(1, "wait on pid %d", pid);

	*error_code = WEXITSTATUS(status);
	*output_size = nbytes;
	return output;
}

/*
 * cmd_pipe_stdin - attach an input string to stdin and execute a utility
 */
int
cmd_pipe_stdin(char *const argv[], char *input, size_t len) {
	int status;
	int stdin_pipe[2];
	pid_t pid;

	pipe(stdin_pipe);
	pid = fork();
	if (pid == -1)
		err(1, "fork");

	if (pid == 0) {
		close(stdin_pipe[1]);
		dup2(stdin_pipe[0], STDIN_FILENO);
		execvp(argv[0], argv);
		err(1, "could not exec %s", argv[0]);
	}
	close(stdin_pipe[0]);
	if (write(stdin_pipe[1], input, len) == -1)
		err(1, "write to child");
	close(stdin_pipe[1]);
	if (waitpid(pid, &status, 0) == -1)
		err(1, "wait on pid %d", pid);

	return WEXITSTATUS(status);
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
 *  verify_ssh_agent - ensure ssh-agent is loaded with at least one unlocked key
 *  start_connection - start an SSH control master and copy _rutils
 *  ssh_command_pipe - execute a script over a pipe to a remote interpreter
 *  ssh_command_tty  - copy script to remote host before execution
 *  end_connection   - stop an SSH control master and remove temporary files
 */

int
verify_ssh_agent() {
	int error_code;
	int output_size;
	char *output;
	char *argv[32];

	append(argv, 0, "ssh-add", "-l", NULL);
	output = cmd_pipe_stdout(argv, &error_code, &output_size);
	free(output);

	return error_code;
}

int
start_connection(char *socket_path, char *host_name, Label *route_label, int http_port, const char *ssh_config) {
	int argc;
	char cmd[PATH_MAX];
	char tmp_path[64];
	char port_forwarding[64];
	char paths[2048];
	char *argv[32];
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
	snprintf(port_forwarding, 64, "%d:localhost:%d", INSTALL_PORT, http_port);

	if (stat(socket_path, &sb) != -1) {
		fprintf(stderr, "rset: socket for '%s' already exists, run\n"
		    "  fstat %s\n"
		    "and remove the file if no process is listed.\n",
		    host_name, socket_path);
		return -1;
	}

	argc = 0;
	argc = append(argv, argc, "ssh", "-fN", "-R", port_forwarding, "-S",
		socket_path, "-M", NULL);
	if (ssh_config)
		(void) append(argv, argc, "-F", ssh_config, host_name, NULL);
	else
		(void) append(argv, argc, host_name, NULL);
	if (run(argv) == 255)
		return -1;

	snprintf(tmp_path, sizeof(tmp_path), "mkdir " REMOTE_TMP_PATH, http_port);
	append(argv, 0, "ssh", "-S", socket_path, host_name, tmp_path, NULL);
	if (run(argv) != 0)
		return -1;

	array_to_str(route_label->export_paths, paths, sizeof(paths), " ");
	snprintf(cmd, PATH_MAX, "tar " TAR_OPTIONS " -cf - %s "
        "-C " REPLICATED_DIRECTORY " ./ | "
	   "exec ssh -q -S %s %s tar -xf - -C " REMOTE_TMP_PATH,
	    paths, socket_path, host_name, http_port);
	if (system(cmd) != 0) {
		warn("transfer failed for " REPLICATED_DIRECTORY);
		return -1;
	}

	return 0;
}

int
update_environment_file(char *host_name, char *socket_path, Label *host_label, int http_port) {
	int fd;
	char cmd[PATH_MAX];
	char tmp_src[128];
	size_t len;
	Options op;
	static char environment_set[PLN_OPTION_SIZE] = "";
	static char environment_file_set[PLN_OPTION_SIZE] = "";
	static char host_name_set[PLN_LABEL_SIZE] = "";

	apply_default(op.environment, host_label->options.environment, ENVIRONMENT);
	apply_default(op.environment_file, host_label->options.environment_file, ENVIRONMENT_FILE);

	/* only update when value changes */
	if ((strcmp(host_name_set, host_name) == 0) &&
	    (strcmp(environment_set, op.environment) == 0) &&
	    (strcmp(environment_file_set, op.environment_file) == 0))
		return 0;

	strlcpy(host_name_set, host_name, PLN_OPTION_SIZE);
	len = strlcpy(environment_set, op.environment, PLN_OPTION_SIZE);
	strlcpy(environment_file_set, op.environment_file, PLN_OPTION_SIZE);

	strlcpy(tmp_src, "/tmp/rset_env_XXXXXX", sizeof tmp_src);
	if ((fd = mkstemp(tmp_src)) == -1)
		err(1, "mkstemp");
	write(fd, environment_set, len);
	write(fd, "\n", 1);
	close(fd);

	snprintf(cmd, PATH_MAX,
	    "cat %s %s | renv | ssh -q -S %s %s 'cat > " REMOTE_TMP_PATH "/final.env'",
	    op.environment_file, tmp_src, socket_path, host_name, http_port);
	if (system(cmd) != 0) {
		warn("unable to read %s", op.environment_file);
		return -1;
	}
	unlink(tmp_src);

	return 0;
}

int
ssh_command_pipe(char *host_name, char *socket_path, Label *host_label, int http_port) {
	int argc;
	char cmd[PATH_MAX];
	char *argv[32];
	Options op;

	/* setup environment file */
	update_environment_file(host_name, socket_path, host_label, http_port);

	/* construct command to execute on remote host  */
	apply_default(op.execute_with, host_label->options.execute_with, EXECUTE_WITH);
	apply_default(op.interpreter, host_label->options.interpreter, INTERPRETER);

	snprintf(cmd, sizeof(cmd), "%s sh -a -c \""
	    "cd " REMOTE_TMP_PATH "; . ./final.env; SD='" REMOTE_TMP_PATH "' INSTALL_URL='" INSTALL_URL "'; exec %s\"",
	    op.execute_with, http_port, http_port, op.interpreter);

	/* construct ssh command */
	argc = 0;
	argc = append(argv, argc, "ssh", "-T", "-S", socket_path, NULL);

	(void) append(argv, argc, host_name, cmd, NULL);
	return cmd_pipe_stdin(argv, host_label->content, host_label->content_size);
}

int
ssh_command_tty(char *host_name, char *socket_path, Label *host_label, int http_port) {
	int argc;
	char cmd[PATH_MAX];
	char *argv[32];
	Options op;

	/* setup environment file */
	update_environment_file(host_name, socket_path, host_label, http_port);

	/* copy the contents of the script */
	snprintf(cmd, sizeof(cmd), "cat > " REMOTE_SCRIPT_PATH,
	    http_port);
	/* construct ssh command */
	argc = 0;
	argc = append(argv, argc, "ssh", "-T", "-S", socket_path, NULL);
	(void) append(argv, argc, host_name, cmd, NULL);
	cmd_pipe_stdin(argv, host_label->content, host_label->content_size);

	/* construct command to execute on remote host  */
	apply_default(op.interpreter, host_label->options.interpreter, INTERPRETER);
	apply_default(op.execute_with, host_label->options.execute_with, EXECUTE_WITH);
	apply_default(op.environment_file, host_label->options.environment_file, ENVIRONMENT_FILE);

	snprintf(cmd, sizeof(cmd), "%s sh -a -c \""
	    "cd " REMOTE_TMP_PATH "; . ./final.env; SD='" REMOTE_TMP_PATH "' INSTALL_URL='" INSTALL_URL "'; exec %s "
	    REMOTE_SCRIPT_PATH "\"",
	    op.execute_with, http_port, http_port, op.interpreter, http_port);

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

	if(access(socket_path, F_OK) == -1)
		return;

	snprintf(tmp_path, sizeof(tmp_path), REMOTE_TMP_PATH, http_port);
	append(argv, 0, "ssh", "-S", socket_path, host_name, "rm", "-rf", tmp_path , NULL);
	if (run(argv) != 0)
		warn("remote tmp dir");

	append(argv, 0, "ssh", "-q", "-S", socket_path, "-O", "exit", host_name, NULL);
	if (run(argv) != 0)
		warn("exec ssh -O exit");
}

/* internal utility functions */

void
apply_default(char *option, const char *user_option, const char *default_option) {
	if (strlen(user_option) > 0)
		memcpy(option, user_option, strlen(user_option)+1);
	else
		memcpy(option, default_option, strlen(default_option)+1);
}
