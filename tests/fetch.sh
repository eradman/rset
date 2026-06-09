#!/bin/sh

src_url=$1
dst=$2

# Select the HTTP client rinstall(1) would use
case `uname` in
	OpenBSD)
		ftp -o $dst -n $src_url
		;;
	FreeBSD)
		fetch -q -o $dst $src_url
		;;
	*)
		if command -v curl > /dev/null; then
			curl -fsLo $dst $src_url
		else
			wget -qO "$dst" "$src_url"
		fi
		;;
esac
