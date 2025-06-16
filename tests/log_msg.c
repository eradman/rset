#include <stdio.h>

#include "input.h"
#include "rutils.h"

/* globals */
Label **route_labels;

int
main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: ./log template [S]\n");
		return 1;
	}

	if ((argc == 3) && (argv[2][0] == 'S'))
		generate_session_id();

	log_msg(argv[1], "localhost", "network", 2);

	return 0;
}
