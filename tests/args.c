#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "config.h"
#include "execute.h"

/* globals */
Label **route_labels;

int
main(int argc, char *argv[]) {
	int i;
	int cmd_argc;
	char *cmd_argv[32];

	cmd_argv[0] = "echo";
	cmd_argc = 1;
	cmd_argc = append(cmd_argv, cmd_argc, "ssh", "-fnNT", "-R", "6000:localhost:65321", "-S", NULL);
	append(cmd_argv, cmd_argc, "/tmp/rset_control_172.16.0.5", "-M", "172.16.0.5", NULL);
	run(cmd_argv);
	for (i = 0; cmd_argv[i]; i++)
		printf("%d: %s\n", i, cmd_argv[i]);

	return 0;
}
