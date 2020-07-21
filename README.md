Remote Sequential Execution Tool
================================

[rset(1)] operates by staging files on a remote system, then executing
instructions embedded in the [pln(5)] files. Any interpreter capable of running
scripts read over a pipe may be specified.

The bundled utilities [rinstall(1)] and [rsub(1)] provide an easy means of
installing and modifying configuration files, and capabilities are added by
writing utility scripts which are sent along with configuration data.

Source Installation - BSD, Mac OS, and Linux
--------------------------------------------

    ./configure

Compile-time options can be changed by editing `config.h`.

Build using

    make
    make install

or to install locally

    PREFIX=$HOME/local make install

Running Tests
-------------

Ruby 2.4 is required

    make test

News
----

A release history as well as features in the upcoming release are covered in the
[NEWS] file.

[NEWS]: https://raw.githubusercontent.com/eradman/rset/master/NEWS
[rset(1)]: http://scriptedconfiguration.org/man/rset.1.html
[pln(5)]: http://scriptedconfiguration.org/man/pln.5.html
[rinstall(1)]: http://scriptedconfiguration.org/man/rinstall.1.html
[rsub(1)]: http://scriptedconfiguration.org/man/rsub.1.html
