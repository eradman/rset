#!/bin/sh
# A wrapper script for running rset(1) processes in parallel
# Output for each host is written to a separate log file
#
# 2023 Eric Radman <ericshane@eradman.com>

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: RSET_ARGS='...' rset-fanout hostname ..."
	exit 1
}
[ $# -eq 0 ] && usage

# Dry run to ensure routes are valid
rset -n $RSET_ARGS $* > /dev/null || exit 1

# Make or clear log dir
logfile=${RSET_LOGFILE:-log/rset-output}
[ -d log ] || mkdir log
rm -f $logfile.*

# Start up tasks with a random delay based on the number of hosts
let n=0
let argc=$#
while [ $# -gt 0 ]
do
	hostname=$1
	cmd="sleep $((RANDOM % $argc)); rset $RSET_ARGS $hostname 2>&1; mv $logfile.$hostname.{,\$?}"
	sh -c "$cmd" > $logfile.$hostname.&
	n=$((n+1)); shift
done

function status {
	clear
	printf ">> \033[37mrset ${RSET_ARGS}\033[0m\n"
	wc -l $logfile.* | grep -v ' total$'
}

# Wait for all log files to return a status code
while ls log/rset-output.*. > /dev/null
do
	status
	sleep 1
done
status
