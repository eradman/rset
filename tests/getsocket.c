#include <stdio.h>

#include "execute.h"

/* globals */
Label **route_labels;    /* parent */
Label **host_labels;     /* child */
Options current_options;

int main(int argc, char *argv[])
{
	printf("%d\n", get_socket());

	return 0;
}
