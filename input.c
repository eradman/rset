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

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"

#include "config.h"
#include "input.h"
#include "execute.h"

#define LABELS_MAX 100
#define BUFSIZE 4096

/* globals */

extern FILE* yyin;
extern char* yyfn;
extern int n_labels;
extern Label **route_labels;    /* parent */
extern Label **host_labels;     /* child */
extern Options current_options;

Label *lp;

void
yylex() {
	int content_allocation = 0;
	int error_code;
	int tfd = 0;
	int local_argc;
	enum {Local, Remote} context;
	unsigned n = 0;
	char tmp_src[128];
	char *line = NULL;
	char *local_argv[PLN_MAX_PATHS];
	size_t linesize = 0;
	ssize_t linelen;
	Options op;

	context = Remote;
	while ((linelen = getline(&line, &linesize, yyin)) != -1) {
		n++;

		/* empty lines and comments */
		if (line[0] == '\n' || line[0] == '#');

		/* leading whitespace */
		else if (line[0] == ' ') {
			fprintf(stderr, "%s: invalid leading character on line %d: '%c'\n",
			    yyfn, n, line[0]);
			exit(1);
		}

		/* { ... } local execution */
		else if (line[0] == '{') {
			context = Local;
			if (strlen(line) > 2) {
				fprintf(stderr, "%s: invalid trailing characters on line %d: '%s'\n",
				    yyfn, n, line);
				exit(1);
			}
			strlcpy(tmp_src, "/tmp/rset_local.XXXXXX", sizeof tmp_src);
			mktemp(tmp_src);
			if ((tfd = open(tmp_src, O_CREAT|O_RDWR, 0600)) == -1)
				err(1, "open %s", tmp_src);
		}

		else if (line[0] == '}') {
			context = Remote;
			if (strlen(line) > 2) {
				fprintf(stderr, "%s: invalid trailing characters on line %d: '%s'\n",
				    yyfn, n, line);
				exit(1);
			}
			if (tfd > 0) {
				close(tfd);
				lp = host_labels[n_labels-1];
				apply_default(op.local_interpreter, lp->options.local_interpreter, LOCAL_INTERPRETER);

				local_argc = str_to_array(local_argv, op.local_interpreter, PLN_MAX_PATHS);
				(void) append(local_argv, local_argc, tmp_src, NULL);

				lp->content = cmd_pipe_stdout(local_argv, &error_code, &content_allocation);
				lp->content_size = strlen(lp->content);

				unlink(tmp_src);
				if (error_code != 0)
					errx(1, "local execution for %s label '%s' exited with code %d",
					    yyfn, lp->name, error_code);
				tfd = 0;

				if ((lp->content_size > 0) && (lp->content[lp->content_size-1] != '\n')) {
					fprintf(stderr, "%s: output of local execution for the label '%s' "
					    "must end with a newline\n", yyfn, lp->name);
					exit(1);
				}
			}
		}

		/* tab-intended content */
		else if (line[0] == '\t') {
			switch (context) {
				case Local:
					if ((write(tfd, line+1, linelen)) == -1)
						err(1, "write");
					break;
				case Remote:
					lp = host_labels[n_labels-1];
					while ((linelen + lp->content_size) >= content_allocation) {
						content_allocation += BUFSIZE;
						lp->content = realloc(lp->content, content_allocation);
					}
					memcpy(lp->content+lp->content_size, line+1, linelen-1);
					lp->content_size += linelen-1;
					lp->content[lp->content_size] = '\0';
					break;
			}
		}

		/* label */
		else if (strchr(line, ':')) {
			host_labels[n_labels] = malloc(sizeof(Label));
			host_labels[n_labels]->content = malloc(BUFSIZE);
			content_allocation = BUFSIZE;
			read_label(line, host_labels[n_labels]);
			n_labels++;
			if (n_labels == LABELS_MAX) {
				fprintf(stderr, "%s: maximum number of labels (%d) "
				    "exceeded\n", yyfn, n_labels );
				exit(1);
			}
		}
		
		/* option */
		else if (strchr(line, '=')) {
			line[linelen-1] = '\0';
			read_option(line, &current_options);
		}

		/* unknown */
		else {
			line[linelen-1] = '\0';
			fprintf(stderr, "%s: unknown symbol at line %d: '%s'\n",
			    yyfn, n, line);
			exit(1);
		}
	}

	free(line);
	if (ferror(yyin))
		err(1, "getline");
}

/*
 * alloc_labels - allocate memory for Label struct
 */
Label**
alloc_labels() {
	Label **new_labels;

	new_labels = malloc(LABELS_MAX * sizeof(Label *));
	bzero(new_labels, LABELS_MAX * sizeof(Label *));

	return new_labels;
}

/*
 * read_host_labels - read a label an its contents
 */
void
read_host_labels(Label *route_label) {
	int j;
	char *line, *next_line;
	char *content;

	host_labels = alloc_labels();
	route_label->labels = host_labels;
	content = strdup(route_label->content);
	line = content;
	n_labels = 0;
	while (*line) {
		next_line = strchr(line, '\n');
		*next_line = '\0';

		/* inherit option state from the routes file */
		memcpy(&current_options, &route_label->options,
		    sizeof(current_options));
		yyfn = line; /* for error message */
		yyin = fopen(line, "r");
		if (!yyin)
			err(1, "%s", line);
		yylex();
		fclose(yyin);
		line = next_line+1;

		/* unsupported options */
		for (j=0; host_labels[j]; j++) {
			if (*host_labels[j]->export_paths != NULL) {
				fprintf(stderr, "%s: label validation error: '%s: %s'\n"
				    "      path export lists are only supported route files\n",
				    yyfn, host_labels[j]->name,
				    array_to_str(host_labels[j]->export_paths));
				exit(1);
			}
		}
	}
	free(content);
}

/*
 * str_to_array - map space-separated tokens to an array using the input string
 *                as the buffer.  If no entries are found, *argv will be NULL
 */
int
str_to_array(char *argv[], char *inputstring, int siz) {
	int argc;
	char **ap;

	argc = 0;
	for (ap = argv; ap < &argv[siz] &&
		(*ap = strsep(&inputstring, " ")) != NULL;) {
			argc++;
			if (**ap != '\0')
				ap++;
	}
	*ap = NULL;
	return argc;
}

/*
 * array_to_str - represent an array as a string, returning a pointer to a
 *                static buffer
 */
char *
array_to_str(char *argv[]) {
	int n = 0;
	static char s[1024];
	char *p = s;

	while (argv && *argv) {
		n += snprintf(p+n, sizeof(s)-n, "%s ", *argv);
		argv++;
	}
	s[n-1] = '\0';

	return s;
}

/*
 * ltrim - strim leading characters
 */

char*
ltrim(char *s, int c) {
	int offset=0;

	while (s[offset] == c && s[offset] != '\0')
		offset++;
	return s + offset;
}

/*
 * read_label - populate label name, export_paths and options
 */
void
read_label(char *line, Label *label) {
	line[strlen(line)-1] = '\0';

	strlcpy(label->name, strsep(&line, ":"), PLN_LABEL_SIZE);
	str_to_array(label->export_paths, strdup(ltrim(line, ' ')), PLN_MAX_PATHS);
	memcpy(&label->options, &current_options, sizeof(current_options));

	label->content_size = 0;
	label->labels = 0;
}

/*
 * read_option - set one of the available options
 */
void
read_option(char *text, Options *op) {
	char *k, *v;

	k = text;
	strsep(&text, "=");
	v = text;

	if (strcmp(k, "execute_with") == 0)
		strlcpy(op->execute_with, v, PLN_OPTION_SIZE);
	else if (strcmp(k, "interpreter") == 0)
		strlcpy(op->interpreter, v, PLN_OPTION_SIZE);
	else if (strcmp(k, "local_interpreter") == 0)
		strlcpy(op->local_interpreter, v, PLN_OPTION_SIZE);
	else {
		fprintf(stderr, "%s: unknown option '%s=%s'\n", yyfn, k, v);
		exit(1);
	}
}
