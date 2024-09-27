#include <stdio.h>

#include "input.h"

/* globals */
Label **route_labels; /* parent */
Label **host_labels;  /* child */

int
main(int argc, char **argv) {
	char *hostlist[PLN_MAX_ALIASES];
	int n;
	int n_hosts;

	if (argc != 2) {
		fprintf(stderr, "usage: ./hostlist hostname\n");
		return 1;
	}

	n_hosts = expand_numeric_range(hostlist, argv[1], 50);
	printf("(%d)\n", n_hosts);
	for (n = 0; hostlist[n]; n++)
		printf("%s\n", hostlist[n]);

	return 0;
}
