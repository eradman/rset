#include <stdio.h>
#include <stdlib.h>

#include "input.h"

/* globals */
FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int main(int argc, char *argv[])
{
	char *env;

	if (argc != 2) {
		fprintf(stderr, "usage: ./format_env '...'\n");
		exit(1);
	}

	env = env_split_lines(argv[1], argv[1]);
	printf("%s", env);

	return 0;
}
