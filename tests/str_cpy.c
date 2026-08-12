#include <stdio.h>
#include <stdlib.h>

#include "rutils.h"

/* globals */
Label **route_labels;

void usage();

void
usage() {
	fprintf(stderr, "usage: ./str_cpy 1|4 string\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	char buf[40];
	const char *input;

	if (argc != 3)
		usage();

	buf[0] = '\0';
	input = argv[2];

	switch (argv[1][0]) {
	case '1':
		str_cpy(buf, input, 10);
		break;
	case '4':
		str_cpy(buf, input, 40);
		break;
	default:
		usage();
	}
	printf("%s", buf);

	return 0;
}
