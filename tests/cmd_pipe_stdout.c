#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	int status;
	char *buf;
	char *cmd_argv[16];

	if (argc != 2) {
		fprintf(stderr, "usage: ./cmd_pipe_stdout command\n");
		return 1;
	}

	cmd_argv[0] = argv[1];
	cmd_argv[1] = NULL;

	buf = malloc(MAX_SCRIPT_SIZE);
	status = cmd_pipe_stdout(cmd_argv, buf, MAX_SCRIPT_SIZE);
	write(1, buf, strlen(buf));
	free(buf);

	return status;
}
