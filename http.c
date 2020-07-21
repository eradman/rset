/* miniquark: see LICENSE file for copyright and license details. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "missing/compat.h"
#include "http.h"

const char *req_field_str[] = {
	[REQ_RANGE]   = "Range",
	[REQ_MOD]     = "If-Modified-Since",
	[REQ_AGENT]   = "User-Agent",
};

const char *req_method_str[] = {
	[M_GET]  = "GET",
	[M_HEAD] = "HEAD",
};

const char *status_str[] = {
	[S_OK]                    = "OK",
	[S_PARTIAL_CONTENT]       = "Partial Content",
	[S_MOVED_PERMANENTLY]     = "Moved Permanently",
	[S_NOT_MODIFIED]          = "Not Modified",
	[S_BAD_REQUEST]           = "Bad Request",
	[S_FORBIDDEN]             = "Forbidden",
	[S_NOT_FOUND]             = "Not Found",
	[S_METHOD_NOT_ALLOWED]    = "Method Not Allowed",
	[S_REQUEST_TIMEOUT]       = "Request Time-out",
	[S_RANGE_NOT_SATISFIABLE] = "Range Not Satisfiable",
	[S_REQUEST_TOO_LARGE]     = "Request Header Fields Too Large",
	[S_INTERNAL_SERVER_ERROR] = "Internal Server Error",
	[S_VERSION_NOT_SUPPORTED] = "HTTP Version not supported",
};

char *
timestamp(time_t t, char buf[TIMESTAMP_LEN])
{
	strftime(buf, TIMESTAMP_LEN, "%a, %d %b %Y %T GMT", gmtime(&t));

	return buf;
}

enum status
http_send_status(int fd, enum status s)
{
	char t[TIMESTAMP_LEN];

	if (dprintf(fd,
	            "HTTP/1.1 %d %s\r\n"
	            "Date: %s\r\n"
	            "Connection: close\r\n"
	            "%s"
	            "Content-Type: text/plain\r\n"
	            "\r\n"
	            "%d %s\n",
	            s, status_str[s], timestamp(time(NULL), t),
	            (s == S_METHOD_NOT_ALLOWED) ? "Allow: HEAD, GET\r\n" : "",
	            s, status_str[s]) < 0) {
		return S_REQUEST_TIMEOUT;
	}

	return s;
}

static void
decode(char src[PATH_MAX], char dest[PATH_MAX])
{
	size_t i;
	unsigned char n;
	char *s;

	for (s = src, i = 0; *s; s++, i++) {
		if (*s == '%' && (sscanf(s + 1, "%2hhx", &n) == 1)) {
			dest[i] = n;
			s += 2;
		} else {
			dest[i] = *s;
		}
	}
	dest[i] = '\0';
}

int
http_get_request(int fd, struct request *r)
{
	size_t hlen, i, mlen;
	ssize_t off;
	char h[HEADER_MAX], *p, *q;

	/* empty all fields */
	memset(r, 0, sizeof(*r));

	/*
	 * receive header
	 */
	for (hlen = 0; ;) {
		if ((off = read(fd, h + hlen, sizeof(h) - hlen)) < 0) {
			return http_send_status(fd, S_REQUEST_TIMEOUT);
		} else if (off == 0) {
			break;
		}
		hlen += off;
		if (hlen >= 4 && !memcmp(h + hlen - 4, "\r\n\r\n", 4)) {
			break;
		}
		if (hlen == sizeof(h)) {
			return http_send_status(fd, S_REQUEST_TOO_LARGE);
		}
	}

	/* remove terminating empty line */
	if (hlen < 2) {
		return http_send_status(fd, S_BAD_REQUEST);
	}
	hlen -= 2;

	/* null-terminate the header */
	h[hlen] = '\0';

	/*
	 * parse request line
	 */

	/* METHOD */
	for (i = 0; i < NUM_REQ_METHODS; i++) {
		mlen = strlen(req_method_str[i]);
		if (!strncmp(req_method_str[i], h, mlen)) {
			r->method = i;
			break;
		}
	}
	if (i == NUM_REQ_METHODS) {
		return http_send_status(fd, S_METHOD_NOT_ALLOWED);
	}

	/* a single space must follow the method */
	if (h[mlen] != ' ') {
		return http_send_status(fd, S_BAD_REQUEST);
	}

	/* basis for next step */
	p = h + mlen + 1;

	/* TARGET */
	if (!(q = strchr(p, ' '))) {
		return http_send_status(fd, S_BAD_REQUEST);
	}
	*q = '\0';
	if (q - p + 1 > PATH_MAX) {
		return http_send_status(fd, S_REQUEST_TOO_LARGE);
	}
	memcpy(r->target, p, q - p + 1);
	decode(r->target, r->target);

	/* basis for next step */
	p = q + 1;

	/* HTTP-VERSION */
	if (strncmp(p, "HTTP/", sizeof("HTTP/") - 1)) {
		return http_send_status(fd, S_BAD_REQUEST);
	}
	p += sizeof("HTTP/") - 1;
	if (strncmp(p, "1.0", sizeof("1.0") - 1) &&
	    strncmp(p, "1.1", sizeof("1.1") - 1)) {
		return http_send_status(fd, S_VERSION_NOT_SUPPORTED);
	}
	p += sizeof("1.*") - 1;

	/* check terminator */
	if (strncmp(p, "\r\n", sizeof("\r\n") - 1)) {
		return http_send_status(fd, S_BAD_REQUEST);
	}

	/* basis for next step */
	p += sizeof("\r\n") - 1;

	/*
	 * parse request-fields
	 */

	/* match field type */
	for (; *p != '\0';) {
		for (i = 0; i < NUM_REQ_FIELDS; i++) {
			if (!strncasecmp(p, req_field_str[i],
			                 strlen(req_field_str[i]))) {
				break;
			}
		}
		if (i == NUM_REQ_FIELDS) {
			/* unmatched field, skip this line */
			if (!(q = strstr(p, "\r\n"))) {
				return http_send_status(fd, S_BAD_REQUEST);
			}
			p = q + (sizeof("\r\n") - 1);
			continue;
		}

		p += strlen(req_field_str[i]);

		/* a single colon must follow the field name */
		if (*p != ':') {
			return http_send_status(fd, S_BAD_REQUEST);
		}

		/* skip whitespace */
		for (++p; *p == ' ' || *p == '\t'; p++)
			;

		/* extract field content */
		if (!(q = strstr(p, "\r\n"))) {
			return http_send_status(fd, S_BAD_REQUEST);
		}
		*q = '\0';
		if (q - p + 1 > FIELD_MAX) {
			return http_send_status(fd, S_REQUEST_TOO_LARGE);
		}
		memcpy(r->field[i], p, q - p + 1);

		/* go to next line */
		p = q + (sizeof("\r\n") - 1);
	}

	return 0;
}

static int
normabspath(char *path)
{
	size_t len;
	int last = 0;
	char *p, *q;

	/* require and skip first slash */
	if (path[0] != '/') {
		return 1;
	}
	p = path + 1;

	/* get length of path */
	len = strlen(p);

	for (; !last; ) {
		/* bound path component within (p,q) */
		if (!(q = strchr(p, '/'))) {
			q = strchr(p, '\0');
			last = 1;
		}

		if (p == q || (q - p == 1 && p[0] == '.')) {
			/* "/" or "./" */
			goto squash;
		} else if (q - p == 2 && p[0] == '.' && p[1] == '.') {
			/* "../" */
			if (p != path + 1) {
				/* place p right after the previous / */
				for (p -= 2; p > path && *p != '/'; p--);
				p++;
			}
			goto squash;
		} else {
			/* move on */
			p = q + 1;
			continue;
		}
squash:
		/* squash (p,q) into void */
		if (last) {
			*p = '\0';
			len = p - path;
		} else {
			memmove(p, q + 1, len - ((q + 1) - path) + 2);
			len -= (q + 1) - p;
		}
	}

	return 0;
}

enum status
http_send_response(int fd, struct request *r)
{
	struct stat st;
	struct tm tm;
	size_t len;
	long lower, upper;
	char realtarget[PATH_MAX], t[TIMESTAMP_LEN];
	char *p, *q;
	const char *err;

	/* make a working copy of the target */
	memcpy(realtarget, r->target, sizeof(realtarget));

	/* normalize target */
	if (normabspath(realtarget)) {
		return http_send_status(fd, S_BAD_REQUEST);
	}

	/* reject hidden target */
	if (realtarget[0] == '.' || strstr(realtarget, "/.")) {
		return http_send_status(fd, S_FORBIDDEN);
	}

	/* stat the target */
	if (stat(RELPATH(realtarget), &st) < 0) {
		return http_send_status(fd, (errno == EACCES) ?
		                        S_FORBIDDEN : S_NOT_FOUND);
	}

	if (S_ISDIR(st.st_mode)) {
		/* add / to target if not present */
		len = strlen(realtarget);
		if (len >= PATH_MAX - 2) {
			return http_send_status(fd, S_REQUEST_TOO_LARGE);
		}
		if (len && realtarget[len - 1] != '/') {
			realtarget[len] = '/';
			realtarget[len + 1] = '\0';
		}
	}

	if (S_ISDIR(st.st_mode))
		return http_send_status(fd, S_FORBIDDEN);

	/* modified since */
	if (r->field[REQ_MOD][0]) {
		/* parse field */
		if (!strptime(r->field[REQ_MOD], "%a, %d %b %Y %T GMT", &tm)) {
			return http_send_status(fd, S_BAD_REQUEST);
		}

		/* compare with last modification date of the file */
		if (difftime(st.st_mtim.tv_sec, mktime(&tm)) <= 0) {
			if (dprintf(fd,
			            "HTTP/1.1 %d %s\r\n"
			            "Date: %s\r\n"
			            "Connection: close\r\n"
				    "\r\n",
			            S_NOT_MODIFIED, status_str[S_NOT_MODIFIED],
			            timestamp(time(NULL), t)) < 0) {
				return S_REQUEST_TIMEOUT;
			}
			return S_NOT_MODIFIED;
		}
	}

	/* range */
	lower = 0;
	upper = st.st_size - 1;
	if (r->field[REQ_RANGE][0]) {
		/* parse field */
		p = r->field[REQ_RANGE];
		err = NULL;

		if (strncmp(p, "bytes=", sizeof("bytes=") - 1)) {
			return http_send_status(fd, S_BAD_REQUEST);
		}
		p += sizeof("bytes=") - 1;

		if (!(q = strchr(p, '-'))) {
			return http_send_status(fd, S_BAD_REQUEST);
		}
		*(q++) = '\0';
		if (p[0]) {
			lower = strtonum(p, 0, LLONG_MAX, &err);
		}
		if (!err && q[0]) {
			upper = strtonum(q, 0, LLONG_MAX, &err);
		}
		if (err) {
			return http_send_status(fd, S_BAD_REQUEST);
		}

		/* check range */
		if (lower < 0 || upper < 0 || lower > upper) {
			if (dprintf(fd,
			            "HTTP/1.1 %d %s\r\n"
			            "Date: %s\r\n"
			            "Content-Range: bytes */%ld\r\n"
			            "Connection: close\r\n"
			            "\r\n",
			            S_RANGE_NOT_SATISFIABLE,
			            status_str[S_RANGE_NOT_SATISFIABLE],
			            timestamp(time(NULL), t),
			            (long)st.st_size) < 0) {
				return S_REQUEST_TIMEOUT;
			}
			return S_RANGE_NOT_SATISFIABLE;
		}

		/* adjust upper limit */
		if (upper >= st.st_size)
			upper = st.st_size-1;
	}

	return resp_file(fd, RELPATH(realtarget), r, &st, lower, upper);
}

enum status
resp_file(int fd, char *name, struct request *r, struct stat *st,
          long lower, long upper)
{
	FILE *fp;
	enum status s;
	ssize_t bread, bwritten;
	long remaining;
	int range;
	char read_buf[16384];
	char *p, t1[TIMESTAMP_LEN], t2[TIMESTAMP_LEN];

	r->bytes_sent = 0;

	/* open file */
	if (!(fp = fopen(name, "r"))) {
		s = http_send_status(fd, S_FORBIDDEN);
		goto cleanup;
	}

	/* seek to lower bound */
	if (fseek(fp, lower, SEEK_SET)) {
		s = http_send_status(fd, S_INTERNAL_SERVER_ERROR);
		goto cleanup;
	}

	/* send header as late as possible */
	range = r->field[REQ_RANGE][0];
	s = range ? S_PARTIAL_CONTENT : S_OK;

	if (dprintf(fd,
	            "HTTP/1.1 %d %s\r\n"
	            "Date: %s\r\n"
	            "Connection: close\r\n"
	            "Last-Modified: %s\r\n"
	            "Content-Type: application/octet-stream\r\n"
	            "Content-Length: %ld\r\n",
	            s, status_str[s], timestamp(time(NULL), t1),
	            timestamp(st->st_mtim.tv_sec, t2),
	            upper - lower + 1) < 0) {
		s = S_REQUEST_TIMEOUT;
		goto cleanup;
	}
	if (range) {
		if (dprintf(fd, "Content-Range: bytes %ld-%ld/%ld\r\n",
		            lower, upper + (upper < 0),
		            (long)st->st_size) < 0) {
			s = S_REQUEST_TIMEOUT;
			goto cleanup;
		}
	}
	if (dprintf(fd, "\r\n") < 0) {
		s = S_REQUEST_TIMEOUT;
		goto cleanup;
	}

	if (r->method == M_GET) {
		/* write data until upper bound is hit */
		remaining = upper - lower + 1;

		while ((bread = fread(read_buf, 1, MIN(sizeof(read_buf),
		                      (size_t)remaining), fp))) {
			if (bread < 0) {
				s = S_INTERNAL_SERVER_ERROR;
				goto cleanup;
			}
			remaining -= bread;
			p = read_buf;
			while (bread > 0) {
				bwritten = write(fd, p, bread);
				if (bwritten <= 0) {
					s = S_REQUEST_TIMEOUT;
					goto cleanup;
				}
				bread -= bwritten;
				r->bytes_sent += bwritten;
				p += bwritten;
			}
		}
	}

cleanup:
	if (fp) {
		fclose(fp);
	}

	return s;
}
