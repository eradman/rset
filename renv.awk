#!/usr/bin/awk -f
# Set up remote environment for rset(1)
# Print only lines that match the format [name="value" ...]
#
# 2023 Eric Radman <ericshane@eradman.com>

# release: ${release}

BEGIN {
	pattern="^[_A-Za-z0-9]+=\".+\"$"
}
# elide subshells and escape sequences
/\\|\$\(|`/ {
	next
}
$0~pattern {
	pos=0

	# collapse extra spaces and expand literals
	gsub(/[ \t]+/, " ")
	gsub(/\$\$/, "\\$")

	# force non-greedy pattern matching
	len=index($0, "\" ")
	while (len > 0) {
		remaining=substr($0, pos)
		sub(/^ /, "", remaining)
		len=index(remaining, "\" ")
		if (len > 0)
			print substr($0, pos+1, len)
		pos = pos + len + 1
	}

	# single-line match
	match(substr($0, pos), pattern)
	if (RLENGTH > 0)
		print substr($0, pos)
}
