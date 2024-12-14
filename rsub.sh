#!/bin/sh
# A helper utility for rset(1)
# Substitute lines in a file or append if not found

ret=1

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rsub [-A] -r line_regex -l line_text target"
	>&2 echo "usage: rsub target < block_content"
	exit 1
}

trap '' HUP
append=1

while getopts Al:r: arg; do
	case "$arg" in
		A) append=0 ;;
		r) line_regex="$OPTARG" ;;
		l) line_text="$OPTARG" ;;
		?) usage ;;
	esac
done
shift $(($OPTIND - 1))
[ $# -eq 1 ] || usage

target=$1

case $(dirname "$target") in
	/*) ;;
	*)
		>&2 echo "rsub: $target is not an absolute path"
		exit 1
		;;
esac

test -f "$target" || {
	>&2 echo "rsub: file not found: $target"
	exit 3
}

source=$(mktemp rsub_XXXXXXXX)

if test -z "$line_regex$line_text"; then
	start="${RSUB_START:-# start managed block}"
	end="${RSUB_END:-# end managed block}"
	cat <<-EOF > ${source}_b
	${start}
	`cat`
	${end}
	EOF
	awk -v m="$start" -v n="$end" -v block=${source}_b '
	    $0 == m, $0 == n { if (!x) system("cat " block) ; x=1; next; }; 1
	    END { if (!x) system("cat " block) }
	    ' "$target" > $source
	rm ${source}_b
else
	awk -v n=$append -v a="$line_regex" -v b="$line_text" '
	    BEGIN { gsub("&", "\\\\&", b) }
	    { n+=sub(a, b); print }
	    END { if (n==0) print b }
	    ' "$target" > $source
fi

test -e "$target" && diff -U 2 "$target" $source || {
	cp $source "$target"
	ret=0
}

rm -f $source
exit $ret
