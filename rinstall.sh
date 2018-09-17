#!/bin/sh
# A helper utility for rset(1)
# Install files from the local staging area or a remote URL

ret=1

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
target=$2

if test ! -f "$1"; then
	source=$(mktemp XXXXXXXX)
	case `uname` in
		OpenBSD)
			ftp -o $source -n "$INSTALL_URL/$1"
			;;
		FreeBSD)
			fetch -q -o $source "$INSTALL_URL/$1"
			;;
		Linux)
			wget -q -O $source "$INSTALL_URL/$1"
			;;
		Darwin|*)
			which -s wget \
				&& wget -q -O $source "$INSTALL_URL/$1" \
				|| curl -f -s -o $source "$INSTALL_URL/$1"
			;;
	esac

	test $? -eq 0 || {
		>&2 echo "Error fetching $INSTALL_URL/$1"
		exit 3
	}
fi

test -e "$target" && diff -U 2 $target $source || {
	cp $source "$target"
	[ -n "$OWNER" ] && chown $OWNER "$target"
	[ -n "$MODE" ] && chmod $MODE "$target"
	rm -f $source
	ret=0
}
exit $ret
