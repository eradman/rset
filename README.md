Remote Staging Execution Tool
=============================

[rset(1)] operates by staging files on a remote system, then executing
instructions embedded in the [pln(5)] files. Configuration may be written in any
interpreter capable of running input from a pipe.

The bundled utilities [renv(1)], [rinstall(1)] and [rsub(1)] provide standard
means of installing and modifying configuration files. Capabilities are added by
writing utility scripts which are sent along with configuration data.

Supported Platforms
-------------------

- OpenBSD
- FreeBSD
- MacOS
- Linux (glibc, coreutils)
- Linux (musl libc, busybox)

Source Installation
-------------------

    ./configure

Compile-time options can be changed by editing `config.h`.

Build using

    make
    make install

or to install locally

    PREFIX=$HOME/local make install

Running Tests
-------------

The test suite depends on `ruby` and `bundler`.

    make test

Alternatively set a specific version of `ruby`

    make test RUBY=/usr/local/bin/ruby32

Examples
--------

List all labels matching a regex for host 'db1'

    rset -n -x 'net|etc' db1

Execute all labels matching a regex for hosts 'db[1-3]'

    rset -x 'net|etc' db1 db2 db3

Iterate over a list of hosts read from a file and execute labels that start with
default pattern `[0-9a-z]`

    xargs rset < hosts

Show configuration files and labels that contain a reference a specific file

    labelgrep httpd.conf *.pln

News
----

Notification of new releases are provided by an
[Atom feed](https://github.com/eradman/rset/releases.atom),
and release history is covered in the [NEWS](NEWS) file.

[pln(5)]: http://scriptedconfiguration.org/man/pln.5.html
[renv(1)]: http://scriptedconfiguration.org/man/renv.1.html
[rinstall(1)]: http://scriptedconfiguration.org/man/rinstall.1.html
[rset(1)]: http://scriptedconfiguration.org/man/rset.1.html
[rsub(1)]: http://scriptedconfiguration.org/man/rsub.1.html
