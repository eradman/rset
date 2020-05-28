#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "execute.h"

/* globals */

void usage();

void usage() {
	fprintf(stderr, "usage: ./ssh_command S|P|T|E hostname\n"
	    "  S = Start session\n"
	    "  P = Remote Execution over a pipe\n"
	    "  T = Remote execution with TTY\n"
	    "  E = End SSH session\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	char *socket_path;
	Label host_label = { .name="networking" };
	int http_port = 6000;
	char *host_name;
	char* mode;

	/* end_connection calls free() on socket_path */
	socket_path = strdup("/tmp/test_rset_socket");

	if (argc != 3) usage();
	mode = argv[1];
	host_name = argv[2];

	switch (mode[0]) {
	case 'S':
		start_connection(&host_label, http_port, NULL);
		break;
	case 'P':
		ssh_command_pipe(host_name, socket_path, &host_label, http_port);
		break;
	case 'T':
		ssh_command_tty(host_name, socket_path, &host_label, http_port);
		break;
	case 'E':
		end_connection(socket_path, host_name, http_port);
		break;
	default:
		usage();
	};

	return 0;
}
