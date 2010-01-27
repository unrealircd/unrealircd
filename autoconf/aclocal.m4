dnl aclocal.m4 generated automatically by aclocal 1.4-p4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl Macro: unet_CHECK_TYPE_SIZES
dnl
dnl Check the size of several types and define a valid int16_t and int32_t.
dnl
AC_DEFUN(unreal_CHECK_TYPE_SIZES,
[dnl Check type sizes
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
if test "$ac_cv_sizeof_int" = 2 ; then
  AC_CHECK_TYPE(int16_t, int)
  AC_CHECK_TYPE(u_int16_t, unsigned int)
elif test "$ac_cv_sizeof_short" = 2 ; then
  AC_CHECK_TYPE(int16_t, short)
  AC_CHECK_TYPE(u_int16_t, unsigned short)
else
  AC_MSG_ERROR([Cannot find a type with size of 16 bits])
fi
if test "$ac_cv_sizeof_int" = 4 ; then
  AC_CHECK_TYPE(int32_t, int)
  AC_CHECK_TYPE(u_int32_t, unsigned int)
elif test "$ac_cv_sizeof_short" = 4 ; then
  AC_CHECK_TYPE(int32_t, short)
  AC_CHECK_TYPE(u_int32_t, unsigned short)
elif test "$ac_cv_sizeof_long" = 4 ; then
  AC_CHECK_TYPE(int32_t, long)
  AC_CHECK_TYPE(u_int32_t, unsigned long)
else
  AC_MSG_ERROR([Cannot find a type with size of 32 bits])
fi
AC_CHECK_SIZEOF(rlim_t)
if test "$ac_cv_sizeof_rlim_t" = 8 ; then
AC_DEFINE(LONG_LONG_RLIM_T)
fi
])

AC_DEFUN(CHECK_LIBCURL,
[
	AC_ARG_ENABLE(libcurl,
	[AC_HELP_STRING([--enable-libcurl=DIR],[enable libcurl (remote include) support])],
	[
		CURLCFLAG=`$enableval/bin/curl-config --cflags`
		CURLLIBS=`$enableval/bin/curl-config --libs`

		dnl Ok this is ugly, basically we need to strip the version of c-ares that curl uses
		dnl because we want to use our own version (which is hopefully fully binary
		dnl compatible with the curl one as well).
		dnl Therefore we need to strip the cares libs in a weird way...
		dnl If anyone can come up with something better and still portable (no awk!?)
		dnl then let us know.
		if test "x`echo $CURLLIBS |grep ares`" != x ; then
			dnl Attempt one: Linux sed
			XCURLLIBS="`echo "$CURLLIBS"|sed -r 's/(@<:@^ @:>@+ @<:@^ @:>@+ )(@<:@^ @:>@+ @<:@^ @:>@+ )(.+)/\1\3/g' 2>/dev/null`"
			if test x"$XCURLLIBS" = x; then
				dnl Attempt two: FreeBSD (and others?) sed
				XCURLLIBS="`echo "$CURLLIBS"|sed -E 's/(@<:@^ @:>@+ @<:@^ @:>@+ )(@<:@^ @:>@+ @<:@^ @:>@+ )(.+)/\1\3/g' 2>/dev/null`"
				if test x"$XCURLLIBS" = x; then
					AC_MSG_ERROR([sed appears to be broken. It is needed for a remote includes compile hack.])
				fi
			fi
			CURLLIBS="$XCURLLIBS"
		fi
		
		IRCDLIBS="$IRCDLIBS $CURLLIBS"
		CFLAGS="$CFLAGS $CURLCFLAG -DUSE_LIBCURL"
		URL="url.o"
		AC_SUBST(URL)
	])
])

dnl the following 2 macros are based on CHECK_SSL by Mark Ethan Trostler <trostler@juniper.net> 

AC_DEFUN([CHECK_SSL],
[
AC_ARG_ENABLE(ssl,
[AC_HELP_STRING([--enable-ssl=],[enable ssl will check /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr])],
[ 
AC_MSG_CHECKING(for openssl)
    for dir in $enableval /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr; do
        ssldir="$dir"
        if test -f "$dir/include/openssl/ssl.h"; then
	    AC_MSG_RESULT(found in $ssldir/include/openssl)
            found_ssl="yes";
	    if test ! "$ssldir" = "/usr" ; then
                CFLAGS="$CFLAGS -I$ssldir/include";
  	    fi
            break;
        fi
        if test -f "$dir/include/ssl.h"; then
	    AC_MSG_RESULT(found in $ssldir/include)
            found_ssl="yes";
	    if test ! "$ssldir" = "/usr" ; then
	        CFLAGS="$CFLAGS -I$ssldir/include";
	    fi
            break
        fi
    done
    if test x_$found_ssl != x_yes; then
	AC_MSG_RESULT(not found)
	echo ""
	echo "Apparently you do not have both the openssl binary and openssl development libraries installed."
	echo "You have two options:"
	echo "a) Install the needed binaries and libraries"
	echo "   and run ./Config"
	echo "OR"
	echo "b) If you don't need SSL..."
	echo "   Run ./Config and say 'no' when asked about SSL"
	echo ""
	exit 1
    else
        CRYPTOLIB="-lssl -lcrypto";
	if test ! "$ssldir" = "/usr" ; then
           LDFLAGS="$LDFLAGS -L$ssldir/lib";
        fi
	AC_DEFINE(USE_SSL)
    fi
],
)
])

AC_DEFUN([CHECK_ZLIB],
[
AC_ARG_ENABLE(ziplinks,
[AC_HELP_STRING([--enable-ziplinks],[enable ziplinks will check /usr/local /usr /usr/pkg])],
[ 
AC_MSG_CHECKING(for zlib)
    for dir in $enableval /usr/local /usr /usr/pkg; do
        zlibdir="$dir"
        if test -f "$dir/include/zlib.h"; then
	    AC_MSG_RESULT(found in $zlibdir)
            found_zlib="yes";
	    if test "$zlibdir" = "/usr" ; then
		CFLAGS="$CFLAGS -DZIP_LINKS";
	    else
	        CFLAGS="$CFLAGS -I$zlibdir/include -DZIP_LINKS";
	    fi
            break;
        fi
    done
    if test x_$found_zlib != x_yes; then
	AC_MSG_RESULT(not found)
	echo ""
	echo "Apparently you do not have the zlib development library installed."
	echo "You have two options:"
	echo "a) Install the zlib development library"
	echo "   and run ./Config"
	echo "OR"
	echo "b) If you don't need compressed links..."
	echo "   Run ./Config and say 'no' when asked about ziplinks support"
	echo ""
	exit 1
    else
        IRCDLIBS="$IRCDLIBS -lz";
	if test "$zlibdir" != "/usr" ; then
             LDFLAGS="$LDFLAGS -L$zlibdir/lib";
	fi 
        HAVE_ZLIB=yes
    fi
    AC_SUBST(HAVE_ZLIB)
],
)
])
# pkg.m4 - Macros to locate and utilise pkg-config.            -*- Autoconf -*-
# 
# Copyright © 2004 Scott James Remnant <scott@netsplit.com>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# As a special exception to the GNU General Public License, if you
# distribute this file as part of a program that contains a
# configuration script generated by Autoconf, you may include it under
# the same distribution terms that you use for the rest of that program.

# PKG_PROG_PKG_CONFIG([MIN-VERSION])
# ----------------------------------
AC_DEFUN([PKG_PROG_PKG_CONFIG],
[m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^PKG_CONFIG(_PATH)?$])
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config utility])dnl
if test "x$ac_cv_env_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
fi
if test -n "$PKG_CONFIG"; then
	_pkg_min_version=m4_default([$1], [0.9.0])
	AC_MSG_CHECKING([pkg-config is at least version $_pkg_min_version])
	if $PKG_CONFIG --atleast-pkgconfig-version $_pkg_min_version; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		PKG_CONFIG=""
	fi
		
fi[]dnl
])# PKG_PROG_PKG_CONFIG

# PKG_CHECK_EXISTS(MODULES, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Check to see whether a particular set of modules exists.  Similar
# to PKG_CHECK_MODULES(), but does not set variables or print errors.
#
#
# Similar to PKG_CHECK_MODULES, make sure that the first instance of
# this or PKG_CHECK_MODULES is called, or make sure to call
# PKG_CHECK_EXISTS manually
# --------------------------------------------------------------
AC_DEFUN([PKG_CHECK_EXISTS],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
if test -n "$PKG_CONFIG" && \
    AC_RUN_LOG([$PKG_CONFIG --exists --print-errors "$1"]); then
  m4_ifval([$2], [$2], [:])
m4_ifvaln([$3], [else
  $3])dnl
fi])


# _PKG_CONFIG([VARIABLE], [COMMAND], [MODULES])
# ---------------------------------------------
m4_define([_PKG_CONFIG],
[if test -n "$PKG_CONFIG"; then
    if test -n "$$1"; then
        pkg_cv_[]$1="$$1"
    else
        PKG_CHECK_EXISTS([$3],
                         [pkg_cv_[]$1=`$PKG_CONFIG --[]$2 "$3" 2>/dev/null`],
			 [pkg_failed=yes])
    fi
else
	pkg_failed=untried
fi[]dnl
])# _PKG_CONFIG

# _PKG_SHORT_ERRORS_SUPPORTED
# -----------------------------
AC_DEFUN([_PKG_SHORT_ERRORS_SUPPORTED],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])
if $PKG_CONFIG --atleast-pkgconfig-version 0.20; then
        _pkg_short_errors_supported=yes
else
        _pkg_short_errors_supported=no
fi[]dnl
])# _PKG_SHORT_ERRORS_SUPPORTED


# PKG_CHECK_MODULES(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
# [ACTION-IF-NOT-FOUND])
#
#
# Note that if there is a possibility the first call to
# PKG_CHECK_MODULES might not happen, you should be sure to include an
# explicit call to PKG_PROG_PKG_CONFIG in your configure.ac
#
#
# --------------------------------------------------------------
AC_DEFUN([PKG_CHECK_MODULES],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1][_CFLAGS], [C compiler flags for $1, overriding pkg-config])dnl
AC_ARG_VAR([$1][_LIBS], [linker flags for $1, overriding pkg-config])dnl

pkg_failed=no
AC_MSG_CHECKING([for $1])

_PKG_CONFIG([$1][_CFLAGS], [cflags], [$2])
_PKG_CONFIG([$1][_LIBS], [libs], [$2])

m4_define([_PKG_TEXT], [Alternatively, you may set the environment variables $1[]_CFLAGS
and $1[]_LIBS to avoid the need to call pkg-config.
See the pkg-config man page for more details.])

if test $pkg_failed = yes; then
        _PKG_SHORT_ERRORS_SUPPORTED
        if test $_pkg_short_errors_supported = yes; then
	        $1[]_PKG_ERRORS=`$PKG_CONFIG --short-errors --errors-to-stdout --print-errors "$2"`
        else 
	        $1[]_PKG_ERRORS=`$PKG_CONFIG --errors-to-stdout --print-errors "$2"`
        fi
	# Put the nasty error message in config.log where it belongs
	echo "$$1[]_PKG_ERRORS" >&AS_MESSAGE_LOG_FD

	ifelse([$4], , [AC_MSG_ERROR(dnl
[Package requirements ($2) were not met:

$$1_PKG_ERRORS

Consider adjusting the PKG_CONFIG_PATH environment variable if you
installed software in a non-standard prefix.

_PKG_TEXT
])],
		[AC_MSG_RESULT([no])
                $4])
elif test $pkg_failed = untried; then
	ifelse([$4], , [AC_MSG_FAILURE(dnl
[The pkg-config script could not be found or is too old.  Make sure it
is in your PATH or set the PKG_CONFIG environment variable to the full
path to pkg-config.

_PKG_TEXT

To get pkg-config, see <http://pkg-config.freedesktop.org/>.])],
		[$4])
else
	$1[]_CFLAGS=$pkg_cv_[]$1[]_CFLAGS
	$1[]_LIBS=$pkg_cv_[]$1[]_LIBS
        AC_MSG_RESULT([yes])
	ifelse([$3], , :, [$3])
fi[]dnl
])# PKG_CHECK_MODULES
