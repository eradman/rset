#!/usr/bin/awk -f
# A pattern searcher for the pln(5) format used by rset(1)
# Displays the source file, label name, and contents that match
#
# 2019 Eric Radman <ericshane@eradman.com>

function hl_label(s) { return "\033[36m"s"\033[0m" } # cyan
function hl_ln(s) { return "\033[33m"s"\033[0m" }    # yellow
function hl_match(s) { return "\033[4m"s"\033[0m" }  # underline

BEGIN {
	pattern=ARGV[1]; ARGV[1]=""
	if (ARGC < 3) {
		print "release: ${release}" > "/dev/stderr"
		print "usage: labelgrep pattern file.pln [...]" > "/dev/stderr"
		exit 1
	}
	prev_label=""
}

# Skip comments and options
/^#/ { next; }
/^[_a-z]+=/ { next; }

{ tab=0; }
/^\t/ { tab=1; }
/^.+:/ {
	if (tab == 0) {
		split($0, a, ":")
		label = a[1]
		next
	}
}
$0~pattern {
	if (FILENAME label != FILENAME prev_label) {
		print FILENAME, "(" hl_label(label) ")"
		prev_label = label
	}
	match($0, pattern)
	print hl_ln(FNR) \
	      substr($0, 0, RSTART-1) \
	      hl_match(substr($0, RSTART, RLENGTH)) \
	      substr($0, RSTART+RLENGTH)
}
