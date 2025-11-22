#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"

#include "rutils.h"
#include "xlibc.h"

/* forwards */
void indent(int level);
char *quote(char *str);
char *array_to_json(char *argv[]);
char *str_or_empty(char *s);

/* globals */
Label **route_labels;
extern const char *yyfn;
extern FILE *yyin;

void usage();

void
usage() {
	fprintf(stderr,
	    "usage:\n"
	    "  ./parser R routes_file\n"
	    "  ./parser H hosts_file\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int i, j;
	char *fn;
	char *mode;
	Label **host_labels;

	if (argc != 3)
		usage();

	mode = argv[1];
	fn = argv[2];

	route_labels = alloc_labels();
	switch (mode[0]) {
	case 'R':
		read_route_labels(fn);
		break;
	case 'H':
		route_labels[0] = xmalloc(sizeof(Label), "route_labels[]");
		bzero(route_labels[0], sizeof(Label));
		strlcpy(route_labels[0]->name, fn, PLN_LABEL_SIZE);
		route_labels[0]->labels = alloc_labels();
		route_labels[0]->labels[0] = xmalloc(sizeof(Label), "route_labels[].labels[]");
		yyfn = fn;
		yyin = fopen(fn, "r");
		parse_pln(route_labels[0]->labels);
		break;
	}
	chdir(xdirname(fn));

	printf("[\n");
	for (i = 0; route_labels[i]; i++) {
		switch (mode[0]) {
		case 'R':
			read_host_labels(route_labels[i]);
			break;
		}
		host_labels = route_labels[i]->labels;

		if (i > 0)
			printf(",\n");
		indent(1);
		printf("{\n");
		indent(2);
		printf("\"aliases\": %s,\n", array_to_json(route_labels[i]->aliases));
		indent(2);
		printf("\"export_paths\": %s,\n", array_to_json(route_labels[i]->export_paths));

		indent(2);
		printf("\"labels\": [\n");
		for (j = 0; host_labels[j]; j++) {
			if (j > 0)
				printf(",\n");
			indent(3);
			printf("{\n");
			indent(4);
			printf("\"name\": \"%s\",\n", host_labels[j]->name);
			indent(4);
			printf("\"content_size\": %d,\n", host_labels[j]->content_size);
			indent(4);
			printf("\"options\": {\n");
			indent(5);
			printf("\"environment\": \"%s\",\n", quote(host_labels[j]->options.environment));
			indent(5);
			printf("\"environment_file\": \"%s\",\n", host_labels[j]->options.environment_file);
			indent(5);
			printf("\"interpreter\": \"%s\",\n", host_labels[j]->options.interpreter);
			indent(5);
			printf("\"local_interpreter\": \"%s\",\n", host_labels[j]->options.local_interpreter);
			indent(5);
			printf("\"execute_with\": \"%s\",\n", host_labels[j]->options.execute_with);
			indent(5);
			printf("\"begin\": \"%s\",\n", str_or_empty(host_labels[j]->options.begin));
			indent(5);
			printf("\"end\": \"%s\"\n", str_or_empty(host_labels[j]->options.end));
			indent(4);
			printf("}\n");
			indent(3);
			printf("}");
		}
		printf("\n");
		indent(2);
		printf("]\n");
		indent(1);
		printf("}");
	}
	printf("\n]\n");

	return 0;
}

/* JSON functions */

void
indent(int level) {
	if (level > 6)
		level = 6;
	printf("%.*s", level * 2, "            ");
}

char *
array_to_json(char *argv[]) {
	static char outputstring[PLN_LABEL_SIZE];
	int size = PLN_LABEL_SIZE;
	int count = 0;
	char *p = outputstring;

	count += strlcpy(p + count, "[", size - count);
	while (argv && *argv) {
		count += strlcpy(p + count, "\"", size - count);
		count += strlcpy(p + count, *argv, size - count);
		argv++;
		if (argv && *argv)
			count += strlcpy(p + count, "\", ", size - count);
		else
			count += strlcpy(p + count, "\"", size - count);
	}
	count += strlcpy(p + count, "]", size - count);
	outputstring[count] = '\0';
	return outputstring;
}

char *
str_or_empty(char *s) {
	if (s)
		return s;
	return "";
}

char *
quote(char *inputstring) {
	int i;
	int count;
	static int size = 4096;
	static char *str = NULL;

	if (!str)
		str = xmalloc(size, "str");

	for (i = 0, count = 0; inputstring[i]; i++, count++) {
		if (count > size - 2) {
			size += 4096;
			str = xrealloc(str, size, "str");
		}

		switch (inputstring[i]) {
		case '"':
		case '\\':
			str[count++] = '\\';
		}
		str[count] = inputstring[i];
	}
	str[count] = '\0';
	return str;
}
