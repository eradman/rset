#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "rutils.h"

/* globals */
FILE* yyin;
char* yyfn;
int n_labels;
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

void usage();

void usage() {
	fprintf(stderr, "usage: ./parser H|R filename\n"
	    "  H = Process as a host file\n"
	    "  R = Process as route file\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int i, j;
	char *input_fn;
	char *mode;

	if (argc != 3) usage();
	mode = argv[1];
	input_fn = argv[2];

	n_labels = 0;
	host_labels = alloc_labels();
	yyin = fopen(input_fn, "r");
	if (!yyin)
		perror(input_fn);
	yylex();

	switch (mode[0]) {
	case 'H':
		for (i=0; host_labels[i]; i++) {
			printf("[%d] %s\n", i, host_labels[i]->name);
			printf("%s.\n", host_labels[i]->content);
		}
		break;
	case 'R':
		chdir(dirname(input_fn));

		route_labels = host_labels;
		for (i=0; route_labels[i]; i++) {
			read_host_labels(route_labels[i]);
			printf("%s, content_size: %d\n",
			    route_labels[i]->name, route_labels[i]->content_size);
			printf("%s, options: %s\n",
			    route_labels[i]->name, array_to_str(route_labels[i]->export_paths));
		}
		for (j=0; host_labels[j]; j++) {
			printf("%s, content_size: %d\n",
			    host_labels[j]->name, host_labels[j]->content_size);
			printf("%s, options: %s\n",
			    host_labels[j]->name, format_options(&host_labels[j]->options));
		}
		break;
	defualt:
		usage();
	}

	return 0;
}
