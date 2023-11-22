#!/usr/bin/awk -f
# Set up remote environment for rset(1)
# Print only lines that match the format [name="value" ...]

# release: ${release}

function echo(s) {
	sub(/="$/, "=\"\"", s)
	print s
}

BEGIN {
	if (ENVIRON["ECHO_ARGS"]) {
		printf "# renv"
		for (i=1; i<ARGC; i++) { printf " " ARGV[i] }
		printf "\n"
	}

	pattern="^[_A-Za-z0-9]+=\""
}
# elide subshells and escape sequences
/\\|\$\(|`/ {
	next
}
# strip trailing spaces
{
	sub(/[ ]+$/, "")
}
$0~pattern {
	pos=0

	# collapse extra spaces and expand literals
	gsub(/[ \t]+/, " ")
	gsub(/\$\$/, "\\$")

	# collapse repeating quotes
	gsub(/""/, "\"")

	# force non-greedy pattern matching
	len=index($0, "\" ")
	while (len > 0) {
		remaining=substr($0, pos)
		sub(/^ /, "", remaining)
		len=index(remaining, "\" ")
		if (len > 0)
			echo(substr($0, pos+1, len))
		pos = pos + len + 1
	}

	# single-line match
	match(substr($0, pos), pattern)
	if (RLENGTH > 0)
		echo(substr($0, pos))
}
