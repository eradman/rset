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
	int i, j, l;
	char *mode;

	if (argc != 3) usage();
	mode = argv[1];
	yyfn = argv[2];

	n_labels = 0;
	host_labels = alloc_labels();
	yyin = fopen(yyfn, "r");
	if (!yyin)
		perror(yyfn);
	yylex();

	switch (mode[0]) {
	case 'H':
		for (i=0; host_labels[i]; i++) {
			printf("[%d] %s", i, host_labels[i]->name);
			if (host_labels[i]->n_aliases > 1) {
				printf(" ->");
				for (l=0; l < host_labels[i]->n_aliases; l++)
					printf(" %s", host_labels[i]->aliases[l]);
			}
			printf("\n");
			printf("%s.\n", host_labels[i]->content);
		}
		break;
	case 'R':
		chdir(dirname(yyfn));

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
	default:
		usage();
	}

	return 0;
}
