#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "xlibc.h"

/* globals */
Label **route_labels;

void usage();

void
usage() {
	fprintf(stderr, "usage: ./format_env 'name=\"value\"'\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	char *env;

	if (argc != 2)
		usage();

	env = xstrdup(argv[1], "env");
	env_split_lines(env);
	printf("%s", env);

	return 0;
}
