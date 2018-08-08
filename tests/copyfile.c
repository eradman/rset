#include <stdio.h>

#include "input.h"

/* globals */

FILE* yyin;
int yylex();
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */

int main(int argc, char *argv[])
{
	char *cmd;
	Options op;

	if (argc != 3) {
		fprintf(stderr, "usage: ./copyfile src dst\n");
		return 1;
	}
	install_if_new(argv[1], argv[2]);

	return 0;
}
