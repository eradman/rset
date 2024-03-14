#!/usr/bin/awk -f
# A log parser for rset(1) parallel workers
# Displays a summary of each session

# fields:
#   $1  session ID
#   $2  timestamp
#   $3  execution stage
#   $4  label or hostname
#   $5  error code

function max(a, b) {
	if (a > b) return a
	return b
}

BEGIN {
	if (ARGC < 2) {
		print "release: ${release}" > "/dev/stderr"
		print "usage: rexec-summary file [file ...]" > "/dev/stderr"
		exit 1
	}

	FS = "|"
}
NF != 5 {
	next
}
/^[0-9a-f]{8}/ {
	if ($3=="HOST_CONNECT_ERROR")
		connect_error[$1]++

	if ($3=="HOST_CONNECT")
		connect[$1]=$4

	if ($3=="EXEC_BEGIN")
		exec_begin[$1]++

	if ($3=="EXEC_END")
		exec_end[$1]++

	if ($3=="EXEC_ERROR")
		exec_error[$1]++

	logfile[$1] = FILENAME
}
END {
	# determine the width of the hostname column
	len = 8
	for (id in connect)
		len = max(len, length(connect[id]))

	entries = 0
	for (id in connect) {
		entries++
		printf("%s %-"len+2"s", id, connect[id])

		if (connect_error[id] > 0) {
			printf("connect fail")
		}
		else {
			printf("%d/%d complete", exec_end[id], exec_begin[id])
		}
		printf(" >> " logfile[id])
		printf("\n")
	}

	# rewind
	if (ARGV[ARGC-1] != "/dev/null") {
		printf("\033[%dA", entries)
		fflush(stdout)
	}
}
