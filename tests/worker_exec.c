#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>

#include "missing/compat.h"

#include "input.h"
#include "worker.h"

/* globals */
Label **route_labels; /* parent */
Label **host_labels;  /* child */

int
main(int argc, char **argv) {
	int n_workers, worker_id;
	int status;
	char *logdir;
	char **worker_argv;
	const char *errstr;

	if (argc < 4) {
		fprintf(stderr, "usage: ./worker_exec logdir n_workers [args ...]\n");
		return 1;
	}

	logdir = argv[1];
	n_workers = strtonum(argv[2], 1, 8, &errstr);
	worker_argv = argv + 3;

	for (worker_id = 1; worker_id <= n_workers; worker_id++) {
		printf("%s/%s.%d\n", logdir, get_tmstr(), worker_id);
		exec_worker(logdir, worker_id, worker_argv);
	}
	while (wait(&status) > 0)
		;
	return status;
}
