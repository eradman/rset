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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "missing/compat.h"

#include "input.h"

#define LABELS_MAX 100
#define BUFSIZE 4096

/* globals */

FILE* yyin;
Label **host_labels;
Options current_options;
int n_labels;
Label *lp;

void
yylex() {
	unsigned n = 0;
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;

	while ((linelen = getline(&line, &linesize, yyin)) != -1) {
		n++;

		/* empty lines and comments */
		if (line[0] == '\n' || line[0] == '#');

		/* tab-intended content */
		else if (line[0] == '\t') {
			lp = host_labels[n_labels-1];
			while ((linelen + lp->content_size) >= lp->content_allocation) {
				lp->content_allocation += BUFSIZE;
				lp->content = realloc(lp->content, lp->content_allocation);
			}
			memcpy(lp->content+lp->content_size, line+1, linelen-1);
			lp->content_size += linelen-1;
		}

		/* label */
		else if (line[linelen-2] == ':') {
			host_labels[n_labels] = malloc(sizeof(Label));
			host_labels[n_labels]->content = malloc(BUFSIZE);
			host_labels[n_labels]->content_allocation = BUFSIZE;
			host_labels[n_labels]->content_size = 0;
			host_labels[n_labels]->labels = 0;
			memcpy(&host_labels[n_labels]->options, &current_options,
			    sizeof(current_options));
			strlcpy(host_labels[n_labels]->name, line, PATH_MAX);
			host_labels[n_labels]->name[linelen-2] = '\0';
			n_labels++;
			if (n_labels == LABELS_MAX) {
				fprintf(stderr, "Error: maximum number of labels (%d) "
				    "exceeded\n", n_labels);
				exit(1);
			}
		}
		
		/* option */
		else if (strchr(line, '=')) {
			line[linelen-1] = '\0';
			read_option(line, &current_options);
		}

		/* uknown */
		else {
			line[linelen-1] = '\0';
			fprintf(stderr, "rset: unknown symbol at line %d: '%s'\n", n, line);
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
		yyin = fopen(line, "r");
		if (!yyin)
			err(1, "%s", line);
		yylex();
		fclose(yyin);
		line = next_line+1;
	}
	free(content);
}

/*
 * str_to_array - map space-separated tokens to an array
 */
void
str_to_array(char *argv[], char *inputstring, int siz) {
	char **ap;

	for (ap = argv; ap < &argv[siz] &&
		(*ap = strsep(&inputstring, " ")) != NULL;) {
			if (**ap != '\0')
				ap++;
	}
	*ap = NULL;
}

/*
 * read_option - set one of the available options
 */
static void
read_option(char *text, Options *op) {
	char *k, *v;

	k = text;
	strsep(&text, "=");
	v = text;

	if (strcmp(k, "username") == 0)
		strlcpy(op->username, v, sizeof(op->username));
	else if (strcmp(k, "execute_with") == 0)
		strlcpy(op->execute_with, v, sizeof(op->execute_with));
	else if (strcmp(k, "interpreter") == 0)
		strlcpy(op->interpreter, v, sizeof(op->interpreter));
	else if (strcmp(k, "install_url") == 0)
		strlcpy(op->install_url, v, sizeof(op->install_url));
	else {
		fprintf(stderr, "rset: unknown option '%s=%s'\n", k, v);
		exit(1);
	}
}
