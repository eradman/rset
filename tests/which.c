#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "execute.h"

void usage();

/*
 * H = Process as a host file
 * R = Process as route file
 */
void usage() {
	fprintf(stderr, "usage: ./which filename\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	char *prog;
	char *found;

	if (argc != 2) usage();
	prog = argv[1];
	found = findprog(prog, getenv("PATH"));
	printf("%s\n", found);

	return 0;
}
