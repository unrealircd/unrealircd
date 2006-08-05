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
[AC_HELP_STRING([--enable-ssl=],[enable ssl will check /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr])],
[ 
AC_MSG_CHECKING(for openssl)
    for dir in $enableval /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr; do
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
