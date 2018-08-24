#!/bin/sh
# A helper utility for rset(1)
# Install files from a remote URL

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

name=$(basename $1)
target=$2

case `uname` in
	Linux)
		fetch_cmd="wget -q -O $name $INSTALL_URL/$1"
		;;
	Darwin)
		fetch_cmd="curl -s -o $name $INSTALL_URL/$1"
		;;
	*)
		fetch_cmd="ftp -n $INSTALL_URL/$1 -o $name"
		;;
esac
$fetch_cmd || {
	>&2 echo "Error fetching $INSTALL_URL/$1"
	exit 3
}
[ -e $target ] && diff -U 2 $target $name || {
	mv $name $target
	[ -n "$OWNER" ] && chown $OWNER $target
	[ -n "$MODE" ] && chmod $MODE $target
	ret=0
}
exit $ret
