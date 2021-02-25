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

#include "input.h"

#define ALLOCATION_SIZE 32768

/* forwards */

int append(char *argv[], int argc, char *arg, ...);
int run(char *const argv[]);
char *cmd_pipe_stdout(char *const argv[], int *error_code, int *output_size);
int cmd_pipe_stdin(char *const argv[], char *input, size_t len);
int get_socket();
char *findprog(char *prog);

int verify_ssh_agent();
int start_connection(char *socket_path, Label *route_label, int http_port, const char *ssh_config);
int ssh_command_pipe(char *host_name, char *socket_path, Label *host_label, int http_port);
int ssh_command_tty(char *host_name, char *socket_path, Label *host_label, int http_port);
void end_connection(char *socket_path, char *host_name, int http_port);

void apply_default(char *option, const char *user_option, const char *default_option);

