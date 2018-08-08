#include <stdio.h>
#include <string.h>

#include "execute.h"

int main(int argc, char *argv[])
{
	char *cmd;
	Options op;

	if (argc != 3) {
		fprintf(stderr, "usage: ./sshcmd host_name label_name\n");
		return 1;
	}

	bzero(&op, sizeof(op));

	cmd = ssh_command(argv[1], "/tmp/my_socket", argv[2], &op, 3330);
	printf("%s\n", cmd);

	strlcpy(op.username, "eradman", sizeof(op.username));
	strlcpy(op.ssh_options, "-v", sizeof(op.username));
	strlcpy(op.execute_with, "doas", sizeof(op.execute_with));
	strlcpy(op.interpreter, "/bin/ksh -x", sizeof(op.interpreter));

	cmd = ssh_command(argv[1], "/tmp/my_socket", argv[2], &op, 3331);
	printf("%s\n", cmd);

	return 0;
}
