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
	int j;
	int tfd = 0;
	int local_argc;
	enum {Local, Remote} context;
	unsigned n = 0;
	char tmp_src[128];
	char *aliases;
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
			if ((tfd = mkstemp(tmp_src)) == -1)
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

				local_argc = str_to_array(local_argv, op.local_interpreter, PLN_MAX_PATHS, " ");
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
			for (j=0; j < host_labels[n_labels]->n_aliases; j++) {
				aliases = host_labels[n_labels]->aliases[j];
				if (aliases && aliases[0] == ' ') {
					fprintf(stderr, "%s: invalid leading character for label alias on "
					    "line %d: '%c'\n", yyfn, n, aliases[0]);
					exit(1);
				}
			}
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
				fprintf(stderr, "%s: label validation error: '%s'\n"
				    "      path export lists are only supported route files\n",
				    yyfn, host_labels[j]->name);
				exit(1);
			}
		}
	}
	free(content);
}

/*
 * str_to_array - split a string using the input string as the buffer
 */
int
str_to_array(char *argv[], char *inputstring, int max_elements, const char *delim) {
	int argc;
	char **ap;

	argc = 0;
	for (ap = argv; ap < &argv[max_elements] &&
		(*ap = strsep(&inputstring, delim)) != NULL;) {
			argc++;
			if (**ap != '\0')
				ap++;
	}
	*ap = NULL;
	return argc;
}

/*
 * array_to_str - format an array using the output string as the buffer
 */
int
array_to_str(char *argv[], char *outputstring, int max_length, const char *delim) {
	int argc = 0;
	char *p = outputstring;

	while (argv && *argv) {
		argc += strlcpy(p+argc, *argv, max_length-argc);
		argv++;
		if (argv && *argv)
			argc += strlcpy(p+argc, delim, max_length-argc);
	}
	outputstring[argc] = '\0';
	return argc;
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
 * read_label - populate label name, alias, export_paths and options
 */
void
read_label(char *line, Label *label) {
	line[strlen(line)-1] = '\0';
	strlcpy(label->name, strsep(&line, ":"), PLN_LABEL_SIZE);

	label->n_aliases = str_to_array(label->aliases, label->name, PLN_MAX_ALIASES, ",");
	if (label->n_aliases == PLN_MAX_ALIASES)
		errx(1, "a maximum %d aliases may be specified for label '%s'", PLN_MAX_ALIASES-2, label->name);

	if (label->n_aliases == 1)
		label->n_aliases = expand_numeric_range(label->aliases, label->name, PLN_MAX_ALIASES);

	str_to_array(label->export_paths, strdup(ltrim(line, ' ')), PLN_MAX_PATHS, " ");
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

/*
 * expand_numeric_range - bash-style numeric ranges {n..m}
 */

#define RANGE_SIZE 2
#define MAX_DIGITS 6

int
expand_numeric_range(char **argv, char *input, int max_elements) {
	int ch;
	int group;
	int n;
	int i;
	int seq;
	int pos, out_pos;
	int hostcount;
	const char *errstr;

	int in_range = 0;
	int parts_index = 0;
	char parts[RANGE_SIZE][PLN_LABEL_SIZE];

	int range_index[RANGE_SIZE];
	int range_numeric[RANGE_SIZE];
	char range_string[RANGE_SIZE][MAX_DIGITS];

	bzero(parts, sizeof(parts));
	bzero(range_string, sizeof(range_string));
	for (n=0; n<RANGE_SIZE; n++)
		range_index[n] = 0;
	hostcount = 0;

	for (group=0, pos=0, out_pos=0; input[pos]; pos++) {
		ch = input[pos];

		if (group > RANGE_SIZE)
			errx(1, "maximum of %d groups", RANGE_SIZE/2);

		switch (ch) {
			case '.':
				if (in_range) {
					switch (input[pos+1]) {
						case '.':
							group++; pos++;
							continue;
						default:
							errx(1, "unexpected %c at position %d", ch, pos);
					}
				}
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (in_range) {
					n = group - 1; /* [low, high] */
					if (range_index[n] > MAX_DIGITS-2)
						errx(1, "range %s too large at position %d", range_string[n], pos);
					range_string[n][range_index[n]++] = ch;
					continue;
				}
				break;
			case '{':
				in_range = 1;
				group++;
				n = group - 1; /* [low, high] */
				range_index[n] = 0;
				continue;
			case '}':
				if (!in_range)
					errx(1, "unexpected: %c at position %d", ch, pos);
				in_range = 0;
				parts_index++;
				out_pos = 0;
				continue;
		}

		parts[parts_index][out_pos++] = ch;
	}

	if (parts_index > 0) {
		/* multiple groups may be captures, but only the first is formatted */
		range_numeric[0] = range_numeric[1] = 0;

		for (i=0; i<group; i++) {
			range_numeric[i] = strtonum(range_string[i], 0, 9999, &errstr);
			if (errstr != NULL)
				errx(1, "number out of bounds %s: '%s'", errstr, range_string[i]);
		}

		if ((range_numeric[1] - range_numeric[0]) < 1)
			errx(1, "non-ascending range: %d..%d", range_numeric[0], range_numeric[1]);

		if ((range_numeric[1] - range_numeric[0]) > max_elements)
			errx(1, "maximum range exceeds %d", max_elements);

		for (seq=range_numeric[0]; seq<=range_numeric[1]; seq++) {
			asprintf(&argv[hostcount], "%s%d%s", parts[0], seq, parts[1]);
			hostcount++;
		}
	}
	else {
		argv[0] = (char *)input;
		hostcount = 1;
	}
	argv[hostcount] = NULL;
	return hostcount;
}
