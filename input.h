/*
 * input.h
 * Parse progressive label notation
 */

#include <limits.h>
#include <stdbool.h>

/* data */

#ifndef _RSET_INPUT_H_
#define _RSET_INPUT_H_

#define PLN_LABEL_SIZE 128
#define PLN_OPTION_SIZE 90
#define PLN_MAX_PATHS 32
#define PLN_MAX_ALIASES 4

#define MAX_ENVIRONMENT 20 * 1024

typedef struct {
	char execute_with[PLN_OPTION_SIZE];
	char interpreter[PLN_OPTION_SIZE];
	char local_interpreter[PLN_OPTION_SIZE];
	char environment[PLN_OPTION_SIZE];
	char environment_file[PLN_OPTION_SIZE];
	/* not inherited */
	char *begin;
	char *end;
} Options;

typedef struct Label {
	char name[PLN_LABEL_SIZE];
	char *aliases[PLN_MAX_ALIASES];
	int n_aliases;
	char *export_paths[PLN_MAX_PATHS];
	char *content;
	int content_size;
	Options options;
	struct Label **labels;
} Label;

extern Label **route_labels;

/* forwards */

void erry(const char *fmt, ...);
void parse_pln(Label **host_labels);
void read_route_labels(const char *fn);
void read_host_labels(Label *route_label);
void expand_route_labels();
Label **alloc_labels();

char *ltrim(char *, int);
void read_label(char *, Label *);
void read_option(char *, Options *);
int expand_numeric_range(char **, char *);
char *env_split_lines(const char *, const char *, bool);
char *read_environment_file(const char *);

#endif /* _RSET_INPUT_H_ */
