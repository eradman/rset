#include <stdio.h>

#include "config.h"
#include "input.h"

/* globals */
Label **route_labels;

int
main(int argc, char **argv) {
	char *hostlist[MAX_LABELS];
	int n;
	int n_hosts;

	if (argc != 2) {
		fprintf(stderr, "usage: ./hostlist hostname\n");
		return 1;
	}

	n_hosts = expand_numeric_range(hostlist, argv[1]);
	printf("(%d)\n", n_hosts);
	for (n = 0; hostlist[n]; n++)
		printf("%s\n", hostlist[n]);

	return 0;
}
