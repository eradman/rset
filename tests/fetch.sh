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
		curl -f -s -o $dst $src_url
		;;
esac
