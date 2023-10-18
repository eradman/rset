#include <stdio.h>

#include "input.h"

/* globals */
FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int main(int argc, char **argv)
{
	char *hostlist[PLN_MAX_ALIASES];
	int n;
	int n_hosts;

	if (argc != 2) {
		fprintf(stderr, "usage: ./hostlist hostname\n");
		return 1;
	}

	n_hosts = expand_hostlist(argv[1], hostlist);
	printf("(%d)\n", n_hosts);
	for (n=0; hostlist[n]; n++)
		printf("%s\n", hostlist[n]);

	return 0;
}
