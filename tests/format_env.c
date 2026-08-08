#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "xlibc.h"

/* globals */
Label **route_labels;

void usage();

void
usage() {
	fprintf(stderr,
	    "usage:\n"
	    "  ./format_env k 'name=\"value\"'\n"
	    "  ./format_env F 'file1.env file2.env'\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	char cmd[PATH_MAX];
	char *env;
	char *mode;

	if (argc != 3)
		usage();
	mode = argv[1];
	env = xstrdup(argv[2], "env");

	switch (mode[0]) {
	case 'E':
		env_split_lines(env);
		printf("%s", env);
		break;
	case 'F':
		env_file_check(env);
		snprintf(cmd, PATH_MAX, "cat %s", env);
		system(cmd);
		break;
	}

	return 0;
}
