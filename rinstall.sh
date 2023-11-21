#!/bin/sh
# A helper utility for rset(1)
# Install files from the local staging area or a remote URL

ret=1
: ${INSTALL_URL:=http://localhost:6000}
unset http_proxy

usage() {
	>&2 echo "release: ${release}"
	>&2 echo "usage: rinstall [-m mode] [-o owner]"
	>&2 echo "                source target"
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
[ $# -eq 2 ] || usage

source=$1
[ -d "$2" ] && target="$2/$(basename "$source")" || target=$2

case $(dirname "$target") in
	/*) ;;
	*)
		>&2 echo "rinstall: $target is not an absolute path"
		exit 1
		;;
esac

if [ ! -f "$1" ]; then
	umask 044
	source="$(basename "$1" | tr -C '0-9a-zA-Z_' '_')$(mktemp XXXXXX)"
        case $(uname) in
		OpenBSD|NetBSD)
			ftp -o "$source" -n "$INSTALL_URL/$1"
			;;
		FreeBSD|DragonFly)
			fetch -qo "$source" "$INSTALL_URL/$1"
			;;
		Linux|Darwin|SunOS)
			if command -pv curl > /dev/null; then
				curl -fsSLo "$source" "$INSTALL_URL/$1"
			else
				wget -qO "$source" "$INSTALL_URL/$1"
			fi
			;;
		*)
			>&2 echo "Unknown OS"
                        exit 2
			;;
	esac

	[ $? -eq 0 ] || {
		>&2 echo "rinstall: unable to fetch $INSTALL_URL/$1"
		exit 3
	}
	umask 022
fi

# create: 0 - files are the same, 1 - a new file,  2 - files are different
if [ -e "$target" ]; then
	diff -U 2 "$target" "$source" && create=0 || create=2
else
	create=1
fi

if [ $create -ne 0 ]; then
	cp "$source" "$target" && ret=0 || ret=$?
	[ $create -eq 1 -a $ret -eq 0 ] && echo "rinstall: created $target" || :
fi

[ -z "$OWNER" ] || chown $OWNER "$target"
[ -z "$MODE" ] || chmod $MODE "$target"

exit $ret

# vim:noexpandtab:syntax=sh:ts=4
