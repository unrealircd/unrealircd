#! /bin/sh

set -e

autoheader
aclocal -I m4
automake --add-missing
autoconf
