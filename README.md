Remote Sequential Execution Tool
================================

rset(1) operates by staging files on a remote system, then executing
instructions embedded in a file format known as Progressive Label Notation. Any
interpreter capable of running scripts read over a pipe may be specified. The
bundled utilities rinstall(1) and rsub(1) provide an easy means of installing
and modifying configuration files, and capabilities are added by writing
utility scripts which are sent along with configuration data.

Source Installation - BSD, Mac OS, and Linux
--------------------------------------------

    ./configure

To see available build options run `./configure -h`

By default [darkhttpd] is required to for serving files from the `_sources`
directory. This and other compile-time options can be changed by editing
`Makefile`. Build using

    make
    make install

Running Tests
-------------

Ruby 2.4 is required

    make test

News
----

A release history as well as features in the upcoming release are covered in the
[NEWS] file.

[darkhttpd]: https://unix4lyfe.org/darkhttpd/
[NEWS]: https://raw.githubusercontent.com/eradman/ephemeralpg/master/NEWS
