#!/bin/sh
# A helper utility for rset(1)
# Install files from the local staging area or a remote URL

set_defaults() {
	ret=1           # global exit status
	fetched=0       # set to 1 when source was fetched via HTTP
	samedir=0       # set to 1 when target is $SD (Staging Directory)
	source_local=0  # set to 1 if source is defined with an absolute path
	owner=""
	mode=""
	alt_location=""
	: ${INSTALL_URL:=http://localhost:6000}
	: ${RINSTALL_DIFF_ARGS:="-U 2"}
}

main() {
	set_defaults
	parse_args "$@"
	init
	set_source_target_vars "$arg_src" "$arg_dst"

	# If source does not exist then it was not found on a local file system
	# (with an absolute path) or in the current directory (normally $SD)
	if [ ! -f "$source" ]; then
		if [ $source_local -eq 1 ]; then
			>&2 echo "rinstall: source $source with absolute path does not exist"
			exit 1
		fi
		# If file has not been staged, fetch over HTTP
		# Fail if download fails
		[ -f "$SD/$source" ] || download_source
	fi

	# Source file exists: check the difference between source and target
	check_diff_source_target

	# Install a file only if source and target are different
	install_target

	# Target file exists: try changing owner and/or permissions but do not fail
	set_mode_owner

	exit $ret
}

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rinstall [-a alt_location] [-m mode] [-o owner:group] source [target]"
	exit 1
}

init() {
	if [ -z "$SD" ]; then
		>&2 echo "rinstall: staging directory \$SD is not defined"
		exit 1
	fi
	if ! check_absolute_path "$SD"; then
		>&2 echo "rinstall: $SD is not an absolute path"
		exit 1
	fi

	unset http_proxy
	trap '' HUP
}

parse_args() {
	while getopts m:o:a: arg; do
		case "$arg" in
			o) owner="$OPTARG" ;;
			m) mode="$OPTARG" ;;
			a) alt_location="$OPTARG" ;;
			?) usage ;;
		esac
	done
	shift $(($OPTIND - 1))
	[ $# -eq 1 -o $# -eq 2 ] || usage
	arg_src="$1"
	arg_dst="$2"
}

set_source_target_vars() {
	# Source can be a local file defined by absolute path or remote defined
	# using a relative path
	source="$1"
	src_path="$(dirname $source)"

	if [ -z "$2" ]; then
		# Target is not defined, try source located in $SD

		# Ensure the source has a relative path by removing leading slashes
		src_rel_path="$(echo "$src_path" | sed 's|^/*||')"

		# prepare a relative path for the target in $SD
		[ -d "$SD/$src_rel_path" ] || mkdir -p "$SD/$src_rel_path" || {
			>&2 echo "rinstall: could not create a relative path: $SD/$src_rel_path"
			exit 1
		}

		if [ "$src_rel_path" = "." ]; then
			target="$SD/$(basename "$source")"
		else
			target="$SD/$src_rel_path/$(basename "$source")"
		fi
		samedir=1
	elif [ -d "$2" ]; then
		target="$2/$(basename "$source")"
	else
		target="$2"
	fi

	if ! check_absolute_path "$target"; then
		>&2 echo "rinstall: $target is not an absolute path"
		exit 1
	fi

	if check_absolute_path "$src_path"; then
		source_local=1
	fi
}

download_source() {
	# Source file does not exist on a local file system, thus it is remote
	# and it should not be defined with an absolute path
	if check_absolute_path "$src_path"; then
		>&2 echo "rinstall: absolute path is not allowed for HTTP fetch: $source"
		exit 1
	fi

	# Reconstruct the source's relative path
	[ -d "$SD/$src_path" ] || mkdir -p "$SD/$src_path" || {
		>&2 echo "rinstall: could not create a relative path: $SD/$src_path"
		exit 1
	}

	fetch_file "$INSTALL_URL/$source"
	if [ $? -ne 0 ]; then
		if [ -n "$alt_location" ]; then
			echo "rinstall: using alternate source: $alt_location"
			fetch_file "$alt_location"
			[ $? -eq 0 ] || {
				>&2 echo "rinstall: unable to fetch $alt_location"
				exit 3
			}
		else
			>&2 echo "rinstall: unable to fetch $INSTALL_URL/$source"
			exit 3
		fi
	fi
	fetched=1
}

fetch_file() {
	case $(uname) in
		OpenBSD)
			ftp -o "$SD/$source" -n "$1"
			;;
		FreeBSD)
			fetch -qo "$SD/$source" "$1"
			;;
		*)
			if command -pv curl > /dev/null; then
				curl -fsLo "$SD/$source" "$1"
			else
				wget -qO "$SD/$source" "$1"
			fi
			;;
	esac
	return $?
}

check_diff_source_target() {
	# Set the 'create' flag:
	#   0 - files are the same
	#   1 - a new file
	#   2 - files are different

	# If source was fetched, then the source is located in $SD, otherwise the
	# source is on a local file system
	if [ $fetched -eq 1 -o $source_local -eq 0 ]; then
		src_file="$SD/$source"
	else
		src_file="$source"
	fi

	if [ -e "$target" ]; then
		if [ $samedir -eq 1 ]; then
			create=0
			[ $fetched -eq 0 ] || {
				ret=0
				echo "rinstall: fetched $target"
			}
		else
			if output="$(diff $RINSTALL_DIFF_ARGS "$target" "$src_file" 2>&1)"
			then
				create=0
			else
				case $? in
					1)  create=2
						[ -z "$output" ] || echo "$output"
						;;
					*)  >&2 echo "rinstall: $output"
						exit 1
						;;
				esac
			fi
		fi
	else
		create=1
	fi
}

install_target() {
	if [ $create -ne 0 ]; then
		cp "$src_file" "$target" && ret=0 || {
			>&2 echo "rinstall: could not copy $src_file into $target"
			exit 1
		}
		if [ $create -eq 1 ]; then
			echo "rinstall: created $target"
		fi
	fi
}

set_mode_owner() {
	if [ -n "$owner" ]; then
		chown $owner "$target"
	fi
	if [ -n "$mode" ]; then
		chmod $mode "$target"
	fi
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
