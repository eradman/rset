#!/bin/sh
# A helper utility for rset(1)
# Install files from the local staging area or a remote URL

ret=1
fetched=0
samedir=0
: ${INSTALL_URL:=http://localhost:6000}
unset http_proxy

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rinstall [-m mode] [-o owner] source [target]"
	exit 1
}

trap '' HUP

while getopts m:o: arg; do
	case "$arg" in
		o) OWNER="$OPTARG" ;;
		m) MODE="$OPTARG" ;;
		?) usage ;;
	esac
done
shift $(($OPTIND - 1))
[ $# -eq 1 -o $# -eq 2 ] || usage

source="$1"
spath="$(dirname $source)"

if [ -z "$2" ]; then
	# ensure the source is a relative path by removing leading slashes
	srpath="$(echo "$spath" | sed 's|^/*||')"

	[ -n "$SD" ] || {
		>&2 echo "rinstall: staging directory \$SD is not defined"
		exit 1
	}

	# prepare a relative path for the target
	[ -d "$SD/$srpath" ] || mkdir -p "$SD/$srpath" || {
		>&2 echo "rinstall: could not create a relative path: $srpath"
		exit 1
	}

	if [ "$srpath" = "." ]; then
		target="$SD/$(basename "$source")"
	else
		target="$SD/$srpath/$(basename "$source")"
	fi
	samedir=1
elif [ -d "$2" ]; then
	target="$2/$(basename "$source")"
else
	target="$2"
fi

case $(dirname "$target") in
	/*) ;;
	*)
		>&2 echo "rinstall: $target is not an absolute path"
		exit 1
		;;
esac

# If a source file doesn't exist, try to download it
if [ ! -f "$source" ]; then
	# The source file doesn't exist at this point and it shouldn't be
	# an absolute path in this case
	case $spath in
		/*)
			>&2 echo "rinstall: absolute path not permitted when fetching over HTTP: $source"
			exit 1
			;;
		*)	;;
	esac

	# reconstruct the source's relative path
	[ -d "$spath" ] || mkdir -p "$spath" || {
		>&2 echo "rinstall: could not create a relative path: $spath"
		exit 1
	}

	case $(uname) in
		OpenBSD|NetBSD)
			ftp -o "$source" -n "$INSTALL_URL/$source"
			;;
		FreeBSD|DragonFly)
			fetch -qo "$source" "$INSTALL_URL/$source"
			;;
		Linux|Darwin|SunOS)
			if command -pv curl > /dev/null; then
				curl -fsSLo "$source" "$INSTALL_URL/$source"
			else
				wget -qO "$source" "$INSTALL_URL/$source"
			fi
			;;
		*)
			>&2 echo "Unknown OS: $(uname)"
			exit 2
			;;
	esac

	[ $? -eq 0 ] || {
		>&2 echo "rinstall: unable to fetch $INSTALL_URL/$source"
		exit 3
	}
	fetched=1
fi

# Source file exists at this point
# create: 0 - files are the same, 1 - a new file,  2 - files are different
if [ -e "$target" ]; then
	if [ $samedir -eq 1 ]; then
		create=0
		[ $fetched -eq 0 ] || {
			ret=0
			echo "rinstall: fetched $target"
		}
	else
		diff -U 2 "$target" "$source" && create=0 || create=2
	fi
else
	create=1
fi

# Copy file only if source and target are different
if [ $create -ne 0 ]; then
	cp "$source" "$target" && ret=0 || {
		>&2 echo "rinstall: could not copy $source into $target"
		exit 1
	}
	[ $create -eq 1 ] && echo "rinstall: created $target" || :
fi

# Target file exists at this point
# Try to change an owner and/or permissions but do not fail
[ -z "$OWNER" ] || chown $OWNER "$target" || :
[ -z "$MODE" ] || chmod $MODE "$target" || :

exit $ret

# vim:noexpandtab:syntax=sh:ts=4
