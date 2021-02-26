#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
	int error_code;
	int fd;
	int output_size;
	char *output;

	if (argc < 2) {
		fprintf(stderr, "usage: ./cmd_pipe_stdout cmd [args ...]\n");
		return 1;
	}

	output = cmd_pipe_stdout(argv+1, &error_code, &output_size);
	write(1, output, output_size);
	fprintf(stderr, "output_size: %d\n", output_size);
	fprintf(stderr, "strlen: %lu\n", strlen(output));
	free(output);

	return error_code;
}
