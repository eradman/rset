/*
 * execute.c
 * SSH session management and function for execution
 */

#include "input.h"

#define ALLOCATION_SIZE 32768

/* forwards */

char *stagedir();
int run(char *const[]);
char *cmd_pipe_stdout(char *const[], int *, int *);
int cmd_pipe_stdin(char *const[], char *, size_t);
int get_socket();
char *findprog(char *);

int verify_ssh_agent();
int start_connection(char *, char *, Label *, int, const char *);
int update_environment_file(char *, char *, Label *, const char *);
int ssh_command_pipe(char *, char *, Label *, const char *);
int ssh_command_tty(char *, char *, Label *, const char *);
int scp_archive(char *, char *, Label *, bool);
void end_connection(char *, char *);
int local_exec(Label *, char *);

void apply_default(char *, const char *, const char *);
