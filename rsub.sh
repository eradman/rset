#!/bin/sh
# A helper utility for rset(1)
# Subsitute lines in a file or append if not found

ret=1

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rsub [-A] file pattern text"
	exit 1
}

trap '' HUP
APPEND=1

while [ $# -gt 3 ]; do
	case "$1" in
		-A) APPEND=0 ;;
		 *) usage ;;
	esac
	shift
done
[ $# -eq 3 ] || usage

target=$1
source=$(mktemp XXXXXXXX)

test -f "$target" || {
	>&2 echo "rsub: file not found: $target"
	exit 3
}

awk -v n=$APPEND -v a="$2" -v b="$3" \
	"{ n+=sub(a, b)}; {print}; END {if (n==0) print b}" \
	"$target" > $source

test -e "$target" && diff -U 2 "$target" $source || {
	cp $source "$target"
	rm -f $source
	ret=0
}
exit $ret
