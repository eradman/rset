Remote Staging Execution Tool
=============================

[rset(1)] operates by staging files on a remote system, then executing
instructions embedded in the [pln(5)] files. Configuration may be written in any
interpreter capable of executing input from a pipe.  Bundled utilities
[rinstall(1)] and [rsub(1)] provide standard means of installing and modifying
configuration files.

Supported Platforms
-------------------

The following operating systems are fully supported by the build and automated tests:

- OpenBSD
- FreeBSD
- MacOS
- Linux (glibc, coreutils)
- Linux (musl libc, busybox)

Other operating systems may be configured using rset(1) as long as they provide
a Unix-like environment running `sshd` with access to `sh`, `awk` and `tar`.

Source Installation
-------------------

Compile-time options can be changed by editing `config.h`.

Build using

    ./configure
    make
    make install

Or to install locally

    PREFIX=$HOME/local make install

Running Tests
-------------

The test suite depends on `ruby` and `bundler`.

    make test

Examples
--------

List all labels for the host `fs1` matching the default pattern `[0-9a-z]`

    rset -n fs1

Execute all labels matching a regex using a list of hostnames

    rset -x 'etc|nsd' svc1 relay1

Execute configuration using four parallel workers on all hosts with starting
with the name `kube`

    rset -p 4 -o log kube.+

Find labels that contain a reference a specific file

    labelgrep httpd.conf *.pln

News
----

Notification of new releases are provided by an
[Atom feed](https://github.com/eradman/rset/releases.atom),
and release history is covered in the [NEWS](NEWS) file.

[pln(5)]: http://scriptedconfiguration.org/man/pln.5.html
[rinstall(1)]: http://scriptedconfiguration.org/man/rinstall.1.html
[rset(1)]: http://scriptedconfiguration.org/man/rset.1.html
[rsub(1)]: http://scriptedconfiguration.org/man/rsub.1.html
