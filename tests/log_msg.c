#include <stdio.h>

#include "input.h"
#include "rutils.h"

/* globals */
FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: ./log template\n");
		return 1;
	}

	log_msg(argv[1], "localhost", "network");

	return 0;
}
