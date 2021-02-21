#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "execute.h"

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
	int error_code;
	char *output;

	if (argc < 2) {
		fprintf(stderr, "usage: ./cmd_pipe_stdout cmd [args ...]\n");
		return 1;
	}

	output = cmd_pipe_stdout(argv+1, &error_code);
	write(1, output, strlen(output));
	free(output);

	return error_code;
}
