#!/usr/bin/awk -f
# Set up remote environment for rset(1).  Merge input from STDIN and ARGV and
# print only lines that match expected format.
#
# 2023 Eric Radman <ericshane@eradman.com>

# release: ${release}

/`|\\|\$\(/ {
	next
}
/^[_A-Za-z0-9]+=".+"$/ {
	print
}
END {
    # parse space-separated variables
	if (ARGC > 1) {
		gsub(/[_A-Za-z0-9]+=/, "\n&", ARGV[1])
		sub(/[ ]+\n/, "\n", ARGV[1])
		system("echo '" ARGV[1] "' | renv")
	}
}
