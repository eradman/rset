#!/bin/sh

src_url=$1
dst=$2

# Select the HTTP client rinstall(1) would use
case `uname` in
	OpenBSD|NetBSD)
		ftp -o $dst -n $src_url
		;;
	FreeBSD|DragonFly)
		fetch -q -o $dst $src_url
		;;
	Linux)
		wget -q -O $dst $src_url
		;;
	Darwin|SunOS|*)
		curl -f -s -o $dst $src_url
		;;
esac
