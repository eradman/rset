#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "execute.h"

#define MAX_SCRIPT_SIZE 16384

/* globals */
FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int main(int argc, char *argv[])
{
	int fd;
	size_t len;
	char *command, *buf;
	char *cmd_argv[16];

	if (argc != 2) {
		fprintf(stderr, "usage: ./exec input_file\n");
		return 1;
	}

	cmd_argv[0] = "/bin/cat";
	cmd_argv[1] = NULL;
	buf = malloc(MAX_SCRIPT_SIZE);
	fd = open(argv[1], 'r');
	len = read(fd, buf, MAX_SCRIPT_SIZE);
	close(fd);

	pipe_cmd(cmd_argv, buf, len);

	return 0;
}
