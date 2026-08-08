/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <vis.h>

static int
isoctal(int c) {
	u_char uc = c;

	return uc >= '0' && uc <= '7';
}

static int
isvisible(int c, int flag) {
	int vis_sp = flag & VIS_SP;
	int vis_tab = flag & VIS_TAB;
	int vis_nl = flag & VIS_NL;
	int vis_safe = flag & VIS_SAFE;
	int vis_glob = flag & VIS_GLOB;
	int vis_all = flag & VIS_ALL;
	u_char uc = c;

	if (c == '\\' || !vis_all) {
		if ((u_int) c <= UCHAR_MAX && isascii(uc)
		    && ((c != '*' && c != '?' && c != '[' && c != '#') || !vis_glob) && isgraph(uc))
			return 1;
		if (!vis_sp && c == ' ')
			return 1;
		if (!vis_tab && c == '\t')
			return 1;
		if (!vis_nl && c == '\n')
			return 1;
		if (vis_safe && (c == '\b' || c == '\007' || c == '\r' || isgraph(uc)))
			return 1;
	}
	return 0;
}

/*
 * vis - visually encode characters
 */
char *
vis(char *dst, int c, int flag, int nextc) {
	int vis_dq = flag & VIS_DQ;
	int vis_noslash = flag & VIS_NOSLASH;
	int vis_cstyle = flag & VIS_CSTYLE;
	int vis_octal = flag & VIS_OCTAL;
	int vis_glob = flag & VIS_GLOB;

	if (isvisible(c, flag)) {
		if ((c == '"' && vis_dq) || (c == '\\' && !vis_noslash))
			*dst++ = '\\';
		*dst++ = c;
		*dst = '\0';
		return (dst);
	}

	if (vis_cstyle) {
		switch (c) {
		case '\n':
			*dst++ = '\\';
			*dst++ = 'n';
			goto done;
		case '\r':
			*dst++ = '\\';
			*dst++ = 'r';
			goto done;
		case '\b':
			*dst++ = '\\';
			*dst++ = 'b';
			goto done;
		case '\a':
			*dst++ = '\\';
			*dst++ = 'a';
			goto done;
		case '\v':
			*dst++ = '\\';
			*dst++ = 'v';
			goto done;
		case '\t':
			*dst++ = '\\';
			*dst++ = 't';
			goto done;
		case '\f':
			*dst++ = '\\';
			*dst++ = 'f';
			goto done;
		case ' ':
			*dst++ = '\\';
			*dst++ = 's';
			goto done;
		case '\0':
			*dst++ = '\\';
			*dst++ = '0';
			if (isoctal(nextc)) {
				*dst++ = '0';
				*dst++ = '0';
			}
			goto done;
		}
	}
	if (((c & 0177) == ' ') || vis_octal
	    || (vis_glob && (c == '*' || c == '?' || c == '[' || c == '#'))) {
		*dst++ = '\\';
		*dst++ = ((u_char) c >> 6 & 07) + '0';
		*dst++ = ((u_char) c >> 3 & 07) + '0';
		*dst++ = ((u_char) c & 07) + '0';
		goto done;
	}
	if (!vis_noslash)
		*dst++ = '\\';
	if (c & 0200) {
		c &= 0177;
		*dst++ = 'M';
	}
	if (iscntrl((u_char) c)) {
		*dst++ = '^';
		if (c == 0177)
			*dst++ = '?';
		else
			*dst++ = c + '@';
	} else {
		*dst++ = '-';
		*dst++ = c;
	}
done:
	*dst = '\0';
	return (dst);
}

/*
 * strvis, strvisx - visually encode characters from src into dst
 *
 *	Dst must be 4 times the size of src to account for possible
 *	expansion.  The length of dst, not including the trailing NULL,
 *	is returned.
 *
 *	Strvisx encodes exactly len bytes from src into dst.
 *	This is useful for encoding a block of data.
 */
int
strvis(char *dst, const char *src, int flag) {
	char c;
	char *start;

	for (start = dst; (c = *src);)
		dst = vis(dst, c, flag, *++src);
	*dst = '\0';
	return (dst - start);
}

int
strvisx(char *dst, const char *src, size_t len, int flag) {
	char c;
	char *start;

	for (start = dst; len > 1; len--) {
		c = *src;
		dst = vis(dst, c, flag, *++src);
	}
	if (len)
		dst = vis(dst, *src, flag, '\0');
	*dst = '\0';
	return (dst - start);
}
