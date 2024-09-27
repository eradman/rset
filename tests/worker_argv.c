#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "worker.h"

/* globals */
Label **route_labels; /* parent */
Label **host_labels;  /* child */

int
main(int argc, char **argv) {
	int n;
	int worker_argc;
	char **worker_argv;

	if (argc < 2) {
		fprintf(stderr, "usage: ./worker_argv [args ...] hostname\n");
		return 1;
	}

	optind = argc - 1; /* assume last arg is a hostname */
	worker_argv = calloc(sizeof(char *), argc);
	worker_argc = create_worker_argv(argv, worker_argv);

	printf("(%d)\n", worker_argc);
	for (n = 0; n < worker_argc; n++)
		printf("%s\n", worker_argv[n]);

	return 0;
}
