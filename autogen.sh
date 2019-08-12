#!/bin/bash
echo "Regenerating 'configure' and headers..."
echo "NOTE: Normally only UnrealIRCd developers run this command!!"

cd "$(dirname "${0}")"

ACLOCAL_AMFLAGS=(-I autoconf/m4)

aclocal "${ACLOCAL_AMFLAGS[@]}"
autoconf
autoheader
