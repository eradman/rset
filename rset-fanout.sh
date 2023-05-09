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

# Spin up detached session and write a log file for each new window
session=rset
tmux new-session -s $session -d || exit 1
tmux set-hook -t $session -g after-new-window 'pipe-pane "cat > log/rset-output.#W."'

# Start up tasks with a random delay based on the number of hosts
let n=0
let argc=$#
while [ $# -gt 0 ]
do
	hostname=$1
	cmd="sleep $((RANDOM % $argc)); rset $RSET_ARGS $hostname; mv $logfile.$hostname.{,\$?}"
	tmux new-window -t $session:$((n+1)) -n $hostname "$cmd"
	n=$((n+1)); shift
done

# First window no longer needed
tmux kill-window -t $session:0

function status {
	clear
	printf ">> \033[37mrset ${RSET_ARGS}\033[0m\n"
	wc -l $logfile.* | grep -v ' total$'
}

# Wait for all windows to clear
while tmux list-windows -t rset -F '#{window_name}' 2>&1
do
	status
	sleep 1
done
status
