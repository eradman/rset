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

%option yylineno

%{
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <libgen.h>

#include "input.h"

#define LABELS_MAX 100
#define BUFSIZE 4096

/* globals */

Options current_options;
int n_labels;
Label *lp;
int len;
%}

%%
\n  /* ignore */
^[_a-z]+=.*$ {
	read_option(yytext, &current_options);
}
^[^\t].+:$ {
	host_labels[n_labels] = malloc(sizeof(Label));
	host_labels[n_labels]->content = malloc(BUFSIZE);
	host_labels[n_labels]->content_allocation = BUFSIZE;
	host_labels[n_labels]->content_size = 0;
	host_labels[n_labels]->labels = 0;
	memcpy(&host_labels[n_labels]->options, &current_options, sizeof(current_options));
	len = strlcpy(host_labels[n_labels]->name, ltrim(yytext, '\n'), PATH_MAX);
	host_labels[n_labels]->name[len-1] = '\0';
	n_labels++;
	if (n_labels == LABELS_MAX) {
		fprintf(stderr, "Error: maximum number of labels (%d) exceeded\n", n_labels);
		exit(1);
	}
}
^\t.*\n {
	len = strlen(yytext+1);
	lp = host_labels[n_labels-1];
	while ((len + lp->content_size) >= lp->content_allocation) {
		lp->content_allocation += BUFSIZE;
		lp->content = realloc(lp->content, lp->content_allocation);
	}
	memcpy(lp->content+lp->content_size, yytext+1, len);
	lp->content_size += len;
}
^#.*$ /* comment */
^.+$ {
	fprintf(stderr, "unknown token at line %d: '%s'\n", yylineno, yytext);
	exit(1);
}
%%

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

		yyin = fopen(line, "r");
		if (!yyin)
			err(1, "%s", line);
		yylex();
		fclose(yyin);
		line = next_line+1;
	}
}

/*
 * install_if_new - install a file and parent directory if it does not already exist
 */
void
install_if_new(const char *src, const char *dst) {
	int pid;
	int status;
	mode_t dir_mode;
	struct stat src_sb;
	struct stat dst_sb;

	if (stat(src, &src_sb) == -1)
		err(1, "%s", src);

	if (stat(dst, &dst_sb) == -1) {
		pid = fork();
		if (pid == 0) {
			dir_mode = 0750;
			mkdir(dirname(dst), dir_mode);
			if (execl("/usr/bin/install", "/usr/bin/install", "-p", src, dst, NULL) != -1)
				err(1, "%s", dst);
		}
		waitpid(pid, &status, 0);
		if (status != 0)
			warnx("copy failed %s -> %s", src, dst);
	}
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

 /* internal utility functions */

static char*
ltrim(char *s, int c) {
	int offset=0;

	while (s[offset] == c && s[offset] != '\0')
		offset++;
	return s + offset;
}

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
