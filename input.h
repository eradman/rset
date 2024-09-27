/*
 * Copyright (c) 2018 Eric Radman <ericshane@eradman.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <limits.h>

/* data */

#ifndef _RSET_INPUT_H_
#define _RSET_INPUT_H_

#define PLN_LABEL_SIZE 128
#define PLN_OPTION_SIZE 64
#define PLN_MAX_PATHS 32
#define PLN_MAX_ALIASES 100

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

extern Label **route_labels; /* parent */
extern Label **host_labels;  /* child */

/* forwards */

void read_pln(const char *fn);
void parse_pln();
void read_host_labels(Label *route_label);
Label **alloc_labels();
int array_to_str(char *[], char *, int, const char *);
int str_to_array(char *[], char *, int, const char *);

char *ltrim(char *, int);
void read_label(char *, Label *);
void read_option(char *, Options *);
int expand_numeric_range(char **, char *, int);
char *env_split_lines(const char *, const char *, int);
char *read_environment_file(const char *);

#endif /* _RSET_INPUT_H_ */
