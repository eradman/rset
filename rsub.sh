#!/bin/sh
# A helper utility for rset(1)
# Substitute lines in a file or append if not found

set_defaults() {
	ret=1     # global exit statu
	append=1  # set to 0 to append text in line-replace mode
	line_regex=""
	line_text=""
	: ${RSUB_START:="# start managed block"}
	: ${RSUB_END:="# end managed block"}
	: ${RSUB_DIFF_ARGS:="-U 2"}
}

main() {
	set_defaults
	parse_args "$@"
	init

	if [ -z "$line_regex$line_text" ]; then
		replace_block
	else
		replace_line
	fi

	# Source file exists: check the difference between source and target
	check_diff_source_target

	rm -f $source
	exit $ret
}

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rsub [-A] -r line_regex -l line_text target"
	>&2 echo "usage: rsub target < block_content"
	exit 1
}

init() {
	if ! check_absolute_path "$target"; then
		>&2 echo "rsub: $target is not an absolute path"
		exit 1
	fi

	[ -f "$target" ] || {
		>&2 echo "rsub: file not found: $target"
		exit 3
	}

	source=$(mktemp rsub_XXXXXXXX)

	trap '' HUP
}

parse_args() {
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
}

check_diff_source_target() {
	[ -e "$target" ] && diff $RSUB_DIFF_ARGS "$target" $source || {
		cp $source "$target"
		ret=0
	}
}

replace_block() {
	cat <<-EOF > ${source}_b
	${RSUB_START}
	`cat`
	${RSUB_END}
	EOF
	awk -v m="$RSUB_START" -v n="$RSUB_END" -v block=${source}_b '
	    $0 == m, $0 == n { if (!x) system("cat " block) ; x=1; next; }; 1
	    END { if (!x) system("cat " block) }
	    ' "$target" > $source
	rm ${source}_b
}

replace_line() {
	awk -v n=$append -v a="$line_regex" -v b="$line_text" '
	    BEGIN { gsub("&", "\\\\&", b) }
	    { n+=sub(a, b); print }
	    END { if (n==0) print b }
	    ' "$target" > $source
}

check_absolute_path() {
	case $(dirname "$1") in
		/*) return 0
			;;
		*)  return 1
			;;
	esac
}

main "$@"

# vim:noexpandtab:syntax=sh:ts=4
