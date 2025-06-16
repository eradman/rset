#include <stdio.h>
#include <stdlib.h>

#include "input.h"

/* globals */
Label **route_labels;

void usage();

void
usage() {
	fprintf(stderr, "usage: ./format_env N|V 'name=\"value\"'\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	char *env;
	char *mode;
	int verify = 0;

	if (argc != 3)
		usage();
	mode = argv[1];

	switch (mode[0]) {
	case 'N':
		verify = 0;
		break;
	case 'V':
		verify = 1;
		break;
	default:
		usage();
	};

	env = env_split_lines(argv[2], argv[2], verify);
	printf("%s", env);

	return 0;
}
