#include <stdio.h>

#include "rutils.h"

/* globals */
FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: ./copyfile src dst\n");
		return 1;
	}
	install_if_new(argv[1], argv[2]);

	return 0;
}
