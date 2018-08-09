#!/bin/sh
# A helper utility for rset(1)
# Install files from a remote URL

let -i ret=1

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rinstall [-m mode] [-o owner]"
	>&2 echo "                source target"
	exit 1
}

trap 'printf "$0: exit code $? on line $LINENO\n" >&2; exit 1' ERR \
	2> /dev/null || exec bash $0 "$@"
trap '' HUP
set +o posix

while [ $# -gt 2 ]; do
	case "$1" in
		-o) OWNER="$2"; shift ;;
		-m) MODE="$2"; shift ;;
		 *) usage ;;
	esac
	shift
done
[ $# == 2 ] || usage

name=$(basename $1)
target=$2


ftp -n $INSTALL_URL/$1 -o $name
[ -e $target ] && diff -C2 -u $target $name || {
	mv $name $target
	[ -n "$OWNER" ] && chown $OWNER $target
	[ -n "$MODE" ] && chmod $MODE $target
	ret=0
}
exit $ret
