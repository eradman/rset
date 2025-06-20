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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"

#include "config.h"
#include "execute.h"
#include "input.h"

#define LABELS_MAX 100
#define BUFSIZE 4096

/* globals from input.h */
extern Label **route_labels;

/* globals */
FILE *yyin;
Label *lp;
Options current_options;
const char *yyfn;
int n_labels;
enum { HostLabel, RouteLabel } pln_mode;

/*
 * Emit an error current PLN
 */
void
erry(const char *fmt, ...) {
	va_list ap;

	fprintf(stderr, "%s: ", yyfn);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	write(STDERR_FILENO, "\n", 1);
	exit(1);
}

void
parse_pln(Label **labels) {
	int content_allocation = 0;
	int error_code;
	int j;
	int tfd = 0;
	int local_argc;
	enum { Local, Remote } context;
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
		if (line[0] == '\n' || line[0] == '#')
			;

		/* leading whitespace */
		else if (line[0] == ' ')
			erry("invalid leading character on line %d: '%c'", n, line[0]);

		/* { ... } local execution */
		else if (line[0] == '{') {
			context = Local;
			if (strlen(line) > 2)
				erry("invalid trailing characters on line %d: '%s'", n, line);

			strlcpy(tmp_src, "/tmp/rset_local.XXXXXX", sizeof tmp_src);
			if ((tfd = mkstemp(tmp_src)) == -1)
				err(1, "open %s", tmp_src);
		}

		else if (line[0] == '}') {
			context = Remote;
			if (strlen(line) > 2)
				erry("invalid trailing characters on line %d: '%s'", n, line);

			if (tfd > 0) {
				close(tfd);
				lp = labels[n_labels - 1];
				apply_default(
				    op.local_interpreter, lp->options.local_interpreter, LOCAL_INTERPRETER);

				local_argc = str_to_array(local_argv, op.local_interpreter, PLN_MAX_PATHS, " ");
				(void) append(local_argv, local_argc, tmp_src, NULL);

				lp->content = cmd_pipe_stdout(local_argv, &error_code, &content_allocation);
				lp->content_size = strlen(lp->content);

				unlink(tmp_src);
				if (error_code != 0)
					errx(1, "local execution for %s label '%s' exited with code %d", yyfn, lp->name,
					    error_code);
				tfd = 0;

				if ((lp->content_size > 0) && (lp->content[lp->content_size - 1] != '\n'))
					erry("output of local execution for the label '%s' must end with a newline",
					    lp->name);
			}
		}

		/* tab-intended content */
		else if (line[0] == '\t') {
			switch (context) {
			case Local:
				if ((write(tfd, line + 1, linelen - 1)) == -1)
					err(1, "write");
				break;
			case Remote:
				lp = labels[n_labels - 1];
				while ((linelen + lp->content_size) >= content_allocation) {
					content_allocation += BUFSIZE;
					lp->content = realloc(lp->content, content_allocation);
				}
				memcpy(lp->content + lp->content_size, line + 1, linelen - 1);
				lp->content_size += linelen - 1;
				lp->content[lp->content_size] = '\0';
				break;
			}
		}

		/* option */
		else if (strchr(line, '=')) {
			line[linelen - 1] = '\0';
			read_option(line, &current_options);
		}

		/* label */
		else if (strchr(line, ':')) {
			labels[n_labels] = malloc(sizeof(Label));
			labels[n_labels]->content = malloc(BUFSIZE);
			content_allocation = BUFSIZE;
			read_label(line, labels[n_labels]);
			for (j = 0; j < labels[n_labels]->n_aliases; j++) {
				aliases = labels[n_labels]->aliases[j];
				if (aliases && aliases[0] == ' ')
					erry("invalid leading character for label alias on line %d: '%c'", n,
					    aliases[0]);
			}
			n_labels++;
			if (n_labels == LABELS_MAX)
				erry("maximum number of labels (%d) exceeded", n_labels);
		}

		/* unknown */
		else {
			line[linelen - 1] = '\0';
			erry("unknown symbol at line %d: '%s'", n, line);
		}
	}

	free(line);
	if (ferror(yyin))
		err(1, "getline");
}

/*
 * alloc_labels - allocate memory for Label struct
 */
Label **
alloc_labels() {
	Label **new_labels;

	new_labels = malloc(LABELS_MAX * sizeof(Label *));
	bzero(new_labels, LABELS_MAX * sizeof(Label *));

	return new_labels;
}

/*
 * read_route_labels - read the routes file
 */
void
read_route_labels(const char *fn) {
	yyfn = fn;
	yyin = fopen(fn, "r");
	if (!yyin)
		err(1, "%s", fn);

	pln_mode = RouteLabel;
	parse_pln(route_labels);
	fclose(yyin);
}

/*
 * read_host_labels - read all pln files referenced in a route label
 */
void
read_host_labels(Label *route_label) {
	char *line, *next_line;
	char *content;

	pln_mode = HostLabel;
	route_label->labels = alloc_labels();
	content = strdup(route_label->content);
	line = content;
	n_labels = 0;
	while (*line) {
		next_line = strchr(line, '\n');
		*next_line = '\0';

		/* inherit option state from the routes file */
		memcpy(&current_options, &route_label->options, sizeof(current_options));
		yyfn = line; /* for error message */
		yyin = fopen(line, "r");
		if (!yyin)
			err(1, "%s", line);
		parse_pln(route_label->labels);
		fclose(yyin);
		line = next_line + 1;
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
	for (ap = argv; ap < &argv[max_elements] && (*ap = strsep(&inputstring, delim)) != NULL;) {
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
		argc += strlcpy(p + argc, *argv, max_length - argc);
		argv++;
		if (argv && *argv)
			argc += strlcpy(p + argc, delim, max_length - argc);
	}
	outputstring[argc] = '\0';
	return argc;
}

/*
 * ltrim - strim leading characters
 */

char *
ltrim(char *s, int c) {
	int offset = 0;

	while (s[offset] == c && s[offset] != '\0')
		offset++;
	return s + offset;
}

/*
 * read_label - populate label name, alias, export_paths and options
 */
void
read_label(char *line, Label *label) {
	int len;
	char *export;

	/* remove trailing newline and split on last ':' */
	line[strlen(line) - 1] = '\0';
	export = strrchr(line, ':');
	*export ++= '\0';
	strlcpy(label->name, line, PLN_LABEL_SIZE);

	label->n_aliases = str_to_array(label->aliases, label->name, PLN_MAX_ALIASES, ",");
	if (label->n_aliases == PLN_MAX_ALIASES)
		errx(1, "> %d aliases specified for label '%s'", PLN_MAX_ALIASES - 2, label->name);

	if (label->n_aliases == 1)
		label->n_aliases = expand_numeric_range(label->aliases, label->name, PLN_MAX_ALIASES);

	len = str_to_array(label->export_paths, strdup(ltrim(export, ' ')), PLN_MAX_PATHS, " ");
	if ((label->export_paths[0] != NULL) && (pln_mode != RouteLabel))
		erry("export path on label '%s' may only be specified in the routes file", label->name);

	if (len == PLN_MAX_PATHS)
		erry("> %d export paths specified for label '%s'", PLN_MAX_PATHS - 1, label->name);

	memcpy(&label->options, &current_options, sizeof(current_options));

	/* options not inherited */
	current_options.begin = 0;
	current_options.end = 0;

	label->content_size = 0;
	label->labels = 0;
}

/*
 * read_option - set one of the available options
 */
void
read_option(char *text, Options *op) {
	char *content;
	char *k, *v;

	int len = 0;

	k = text;
	strsep(&text, "=");
	v = text;

	if (strcmp(k, "execute_with") == 0)
		len = strlcpy(op->execute_with, v, PLN_OPTION_SIZE);
	else if (strcmp(k, "interpreter") == 0)
		len = strlcpy(op->interpreter, v, PLN_OPTION_SIZE);
	else if (strcmp(k, "local_interpreter") == 0)
		len = strlcpy(op->local_interpreter, v, PLN_OPTION_SIZE);
	else if (strcmp(k, "environment") == 0) {
		len = strlcpy(op->environment, v, PLN_OPTION_SIZE);
		free(env_split_lines(op->environment, op->environment, 1));
	} else if (strcmp(k, "environment_file") == 0) {
		if (strlcpy(op->environment_file, v, PLN_OPTION_SIZE) > 0) {
			content = read_environment_file(op->environment_file);
			free(env_split_lines(content, op->environment_file, 1));
			free(content);
		}
	} else if (strcmp(k, "begin") == 0) {
		op->begin = strdup(v);
	} else if (strcmp(k, "end") == 0) {
		op->end = strdup(v);
	} else
		erry("unknown option '%s=%s'", k, v);

	if (len > PLN_OPTION_SIZE)
		erry("option '%s' too long: %d > %d", k, len, PLN_OPTION_SIZE);
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
	for (n = 0; n < RANGE_SIZE; n++)
		range_index[n] = 0;
	hostcount = 0;

	for (group = 0, pos = 0, out_pos = 0; input[pos]; pos++) {
		ch = input[pos];

		if (group > RANGE_SIZE)
			errx(1, "maximum of %d groups", RANGE_SIZE / 2);

		switch (ch) {
		case '.':
			if (in_range) {
				switch (input[pos + 1]) {
				case '.':
					group++;
					pos++;
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
				if (range_index[n] > MAX_DIGITS - 2)
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

		for (i = 0; i < group; i++) {
			range_numeric[i] = strtonum(range_string[i], 0, 9999, &errstr);
			if (errstr != NULL)
				errx(1, "number out of bounds %s: '%s'", errstr, range_string[i]);
		}

		if ((range_numeric[1] - range_numeric[0]) < 1)
			errx(1, "non-ascending range: %d..%d", range_numeric[0], range_numeric[1]);

		if ((range_numeric[1] - range_numeric[0]) > max_elements)
			errx(1, "maximum range exceeds %d", max_elements);

		for (seq = range_numeric[0]; seq <= range_numeric[1]; seq++) {
			asprintf(&argv[hostcount], "%s%d%s", parts[0], seq, parts[1]);
			hostcount++;
		}
	} else {
		argv[0] = (char *) input;
		hostcount = 1;
	}
	argv[hostcount] = NULL;
	return hostcount;
}

/*
 * env_split_lines - translate space delimited name="value" to lines
 *                   optionally pipe to renv(1)
 */

char *
env_split_lines(const char *s, const char *option_value, int verify) {
	int count = 0;
	char *argv[8];
	char *new, *p;
	size_t len;

	len = strlen(s);
	new = malloc(len + 2);
	memcpy(new, s, len);

	/* add trailing newline */
	if (len > 0)
		new[len++] = '\n';
	new[len] = '\0';

	p = new;
	while ((p = strchr(p, '"')) != NULL) {
		p++;
		/* expect a pair of delimiters */
		if ((++count % 2 == 0) && (p[0] == ' '))
			p[0] = '\n';
	}

	if (count % 2 == 1)
		errx(1, "no closing quote: %s", option_value);

	if ((len > 0) && verify) {
		(void) append(argv, 0, "renv", "-", "-q", NULL);
		if (cmd_pipe_stdin(argv, new, len) != 0)
			exit(1);
	}

	return new;
}

/*
 * read_environment_file - basic validation
 */

char *
read_environment_file(const char *environment_file) {
	char *buf;
	int fd;
	size_t len;

	buf = malloc(MAX_ENVIRONMENT);

	if ((fd = open(environment_file, O_RDONLY)) == -1)
		err(1, "%s: %s", yyfn, environment_file);

	len = read(fd, buf, MAX_ENVIRONMENT);
	if (len == MAX_ENVIRONMENT)
		errx(1, "environment file %s exceeds %dkB", environment_file, MAX_ENVIRONMENT / 1024);

	close(fd);
	buf[len] = '\0';
	return buf;
}
