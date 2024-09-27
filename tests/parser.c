#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "missing/compat.h"

#include "rutils.h"

/* forwards */
void indent(int level);
char *quote(char *str);
char * array_to_json(char *argv[]);
char * str_or_empty(char *s);

/* globals */
Label **route_labels;    /* parent */
Label **host_labels;     /* child */

int main(int argc, char *argv[]) {
	int i, j;
	char *yyfn;

	if (argc != 2) {
		fprintf(stderr, "usage: ./parser routes_file\n");
		exit(1);
	}

	yyfn = argv[1];
	host_labels = alloc_labels();
	read_pln(yyfn);
	chdir(xdirname(yyfn));

	route_labels = host_labels;
	printf("[\n");
	for (i=0; route_labels[i]; i++) {
		read_host_labels(route_labels[i]);

		if (i > 0) printf(",\n");
		indent(1); printf("{\n");
		indent(2); printf("\"aliases\": %s,\n", array_to_json(route_labels[i]->aliases));
		indent(2); printf("\"export_paths\": %s,\n", array_to_json(route_labels[i]->export_paths));

		indent(2); printf("\"labels\": [\n");
		for (j=0; host_labels[j]; j++) {
			if (j > 0) printf(",\n");
			indent(3); printf("{\n");
			indent(4); printf("\"name\": \"%s\",\n",  host_labels[j]->name);
			indent(4); printf("\"content_size\": %d,\n", host_labels[j]->content_size);
			indent(4); printf("\"options\": {\n");
			indent(5); printf("\"environment\": \"%s\",\n", quote(host_labels[j]->options.environment));
			indent(5); printf("\"environment_file\": \"%s\",\n", host_labels[j]->options.environment_file);
			indent(5); printf("\"interpreter\": \"%s\",\n", host_labels[j]->options.interpreter);
			indent(5); printf("\"local_interpreter\": \"%s\",\n", host_labels[j]->options.local_interpreter);
			indent(5); printf("\"execute_with\": \"%s\",\n", host_labels[j]->options.execute_with);
			indent(5); printf("\"begin\": \"%s\",\n", str_or_empty(host_labels[j]->options.begin));
			indent(5); printf("\"end\": \"%s\"\n", str_or_empty(host_labels[j]->options.end));
			indent(4); printf("}\n");
			indent(3); printf("}");
		}
		printf("\n");
		indent(2); printf("]\n");
		indent(1); printf("}");
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

	count += strlcpy(p+count, "[", size-count);
	while (argv && *argv) {
		count += strlcpy(p+count, "\"", size-count);
		count += strlcpy(p+count, *argv, size-count);
		argv++;
		if (argv && *argv)
			count += strlcpy(p+count, "\", ", size-count);
		else
			count += strlcpy(p+count, "\"", size-count);
	}
	count += strlcpy(p+count, "]", size-count);
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
		str = malloc(size);

	for (i=0, count=0; inputstring[i]; i++, count++) {
		if (count > size-2) {
			size += 4096;
			str = realloc(str, size);
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
