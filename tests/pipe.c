#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "execute.h"

#define MAX_SCRIPT_SIZE 16384

int main(int argc, char *argv[])
{
	int fd;
	size_t len;
	char *command, *buf;

	if (argc < 3) {
		fprintf(stderr, "usage: ./exec input_file command...\n");
		return 1;
	}

	command = "/bin/cat";
	buf = malloc(MAX_SCRIPT_SIZE);
	fd = open(argv[1], 'r');
	len = read(fd, buf, MAX_SCRIPT_SIZE);
	close(fd);

	pipe_cmd(command, buf, len);

	return 0;
}
