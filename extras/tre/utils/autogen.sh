#! /bin/sh

set -e

libtoolize --automake
autoheader
aclocal -I m4
automake --add-missing
autoconf
