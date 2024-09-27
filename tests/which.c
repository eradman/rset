#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "execute.h"

/* globals */
Label **route_labels; /* parent */
Label **host_labels;  /* child */

void usage();

/*
 * H = Process as a host file
 * R = Process as route file
 */
void
usage() {
	fprintf(stderr, "usage: ./which filename\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	char *prog;
	char *found;

	if (argc != 2)
		usage();
	prog = argv[1];
	found = findprog(prog);
	printf("%s\n", found);

	return 0;
}
