#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "execute.h"

/* globals */
Label **route_labels;

int
main(int argc, char *argv[]) {
	int error_code;
	int output_size;
	char *output;

	if (argc < 2) {
		fprintf(stderr, "usage: ./cmd_pipe_stdout cmd [args ...]\n");
		return 1;
	}

	output = cmd_pipe_stdout(argv + 1, &error_code, &output_size);
	write(1, output, output_size);
	fprintf(stderr, "output_size: %d\n", output_size);
	fprintf(stderr, "strlen: %lu\n", (unsigned long) strlen(output));
	free(output);

	return error_code;
}
