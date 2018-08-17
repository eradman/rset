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

typedef struct {
	char username[32];
	char execute_with[64];
	char interpreter[64];
	char install_url[1024];
} Options;

typedef struct Label Label;

typedef struct Label {
	char name[PATH_MAX];
	char* content;
	int content_size;
	int content_allocation;
	Options options;
	struct Label **labels;
} Label;

extern Label **route_labels;    /* parent */
extern Label **host_labels;     /* child */

/* forwards */

void read_host_labels(Label *route_label);
Label** alloc_labels();
void install_if_new(const char *src, const char *dst);
void str_to_array(char *argv[], char *input, int siz);

static char* ltrim(char *s, int c);
static void read_option(char *text, Options *op);
