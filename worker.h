/*
 * worker.h
 * Functions for parallel execution in rset
 */

#include "input.h"

/* forwards */

int create_worker_argv(char *[], char *[]);
int exec_worker(char *, int, char *[]);
void rexec_summary(int, int[], char *);
int open_log(char *, int);
char *get_tmstr();
