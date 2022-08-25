#!/bin/sh
# A helper utility for rset(1)
# Install files from the local staging area or a remote URL
# 2018 Eric Radman <ericshane@eradman.com>

ret=1
: ${INSTALL_URL:=http://localhost:9000}
unset http_proxy

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rinstall [-m mode] [-o owner]"
	>&2 echo "                source target"
	exit 1
}

trap '' HUP

while [ $# -gt 2 ]; do
	case "$1" in
		-o) OWNER="$2"; shift ;;
		-m) MODE="$2"; shift ;;
		 *) usage ;;
	esac
	shift
done
[ $# -eq 2 ] || usage

source=$1
test -d "$2" && target="$2/$(basename "$source")" || target=$2

case $(dirname "$target") in
	/*) ;;
	*)
		>&2 echo "rinstall: $target is not an absolute path"
		exit 1
		;;
esac

if test ! -f "$1"; then
	umask 044
	source="$(basename "$1" | tr -C '0-9a-zA-Z_' '_')$(mktemp XXXXXX)"
	case `uname` in
		OpenBSD|NetBSD)
			ftp -o "$source" -n "$INSTALL_URL/$1"
			;;
		FreeBSD|DragonFly)
			fetch -q -o "$source" "$INSTALL_URL/$1"
			;;
		Linux)
			wget -q -O "$source" "$INSTALL_URL/$1"
			;;
		Darwin|SunOS|*)
			curl -f -s -o "$source" "$INSTALL_URL/$1"
			;;
	esac

	test $? -eq 0 || {
		>&2 echo "rinstall: unable to fetch $INSTALL_URL/$1"
		exit 3
	}
	umask 022
fi

test -s "$source" || {
	>&2 echo "rinstall: $1 is empty"
	exit 1
}

test -e "$target" && diff -U 2 "$target" "$source" || {
	test -e "$target" || echo "rinstall: created $target"
	cp "$source" "$target" && ret=0
}

[ -n "$OWNER" ] && chown $OWNER "$target"
[ -n "$MODE" ] && chmod $MODE "$target"

exit $ret
