#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "execute.h"
#include "input.h"
#include "rutils.h"

/* globals */
Label **route_labels;

void usage();

void
usage() {
	fprintf(stderr,
	    "usage:\n"
	    "  ./ssh_command S hostname [export_paths]\n" /* Start session */
	    "  ./ssh_command P hostname [env_override]\n" /* Remote execution over a pipe */
	    "  ./ssh_command T hostname [env_override]\n" /* Remote execution with TTY */
	    "  ./ssh_command A hostname [export_paths]\n" /* Archive files */
	    "  ./ssh_command R hostname [export_paths]\n" /* Resore files */
	    "  ./ssh_command E hostname\n");              /* End session */
	exit(1);
}

int
main(int argc, char *argv[]) {
	char *socket_path;
	Label host_label = { .name = "networking" };
	int http_port = 6000;
	char *env_override = 0;
	char *host_name;
	char *mode;

	/* end_connection calls free() on socket_path */
	socket_path = strdup("/tmp/test_rset_socket");

	if (argc < 3 || argc > 4)
		usage();
	mode = argv[1];
	host_name = argv[2];
	bzero(host_label.export_paths, sizeof host_label.export_paths);

	switch (mode[0]) {
	case 'S':
		if (argc == 4)
			str_to_array(host_label.export_paths, strdup(argv[3]), PLN_MAX_PATHS, " ");
		start_connection(socket_path, host_name, &host_label, http_port, NULL);
		break;
	case 'P':
		if (argc == 4)
			env_override = argv[3];
		ssh_command_pipe(host_name, socket_path, &host_label, http_port, env_override);
		break;
	case 'T':
		if (argc == 4)
			env_override = argv[3];
		ssh_command_tty(host_name, socket_path, &host_label, http_port, env_override);
		break;
	case 'A':
		if (argc == 4)
			str_to_array(host_label.export_paths, strdup(argv[3]), PLN_MAX_PATHS, " ");
		scp_archive(host_name, socket_path, &host_label, http_port, false);
		break;
	case 'R':
		if (argc == 4)
			str_to_array(host_label.export_paths, strdup(argv[3]), PLN_MAX_PATHS, " ");
		scp_archive(host_name, socket_path, &host_label, http_port, true);
		break;
	case 'E':
		end_connection(socket_path, host_name, http_port);
		break;
	default:
		usage();
	};

	return 0;
}
