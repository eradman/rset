/*
 * rutils.h
 * Utility functions for rset
 */

#include <inttypes.h>

#include "input.h"

/* forwards */

size_t str_cpy(char *, const char *, size_t);
int str_to_array(char *[], const char *, int, const char *);
int array_to_str(char *[], char *, int, const char *);
int array_append(char *[], int, char *, ...);
unsigned generate_session_id();
unsigned current_session_id();
void check_permissions(const char *);
int create_dir(const char *);
void install_if_new(const char *, const char *);
void hl_range(const char *, const char *, unsigned, unsigned);
void log_msg(char *, char *, char *, int);
void trace_shell(char *);
void trace_exec(char *[]);
void trace_http(const char *);
