#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "execute.h"

/* globals */
Label **route_labels;    /* parent */
Label **host_labels;     /* child */

int main(int argc, char *argv[])
{
	int fd;
	size_t len;
	char *buf;
	char *cmd_argv[16];

	if (argc != 2) {
		fprintf(stderr, "usage: ./cmd_pipe_stdin input_file\n");
		return 1;
	}

	cmd_argv[0] = "/bin/cat";
	cmd_argv[1] = NULL;
	buf = malloc(ALLOCATION_SIZE);
	fd = open(argv[1], 'r');
	len = read(fd, buf, ALLOCATION_SIZE);
	close(fd);

	cmd_pipe_stdin(cmd_argv, buf, len);

	return 0;
}
