#!/bin/bash

cd "$(dirname "${0}")"

ACLOCAL_AMFLAGS=(-I autoconf/m4)

aclocal "${ACLOCAL_AMFLAGS[@]}"
autoconf
autoheader
