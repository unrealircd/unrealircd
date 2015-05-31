#serial 1

dnl Macro: unreal_CHECK_TYPE_SIZES
dnl originally called unet_CHECK_TYPE_SIZES
dnl
dnl Check the size of several types and define a valid int16_t and int32_t.
dnl
AC_DEFUN([unreal_CHECK_TYPE_SIZES],
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
AC_DEFINE([LONG_LONG_RLIM_T], [], [Define if rlim_t is long long])
fi
])

AC_DEFUN([CHECK_LIBCURL],
[
	AC_ARG_ENABLE(libcurl,
	[AC_HELP_STRING([--enable-libcurl=DIR],[enable libcurl (remote include) support])],
	[enable_curl=$enableval],
	[enable_curl=no])

	AS_IF([test "x$enable_curl" != "xno"],
	[
		dnl sane, default directory for Operating System-managed libcURL
		dnl (when --enable-libcurl is passed without any arguments). On
		dnl systems with stuff in /usr/local, /usr/local/bin should already
		dnl be in PATH. On sane systems, this will invoke the curl-config
		dnl installed by the package manager.
		CURLCONFIG="curl-config"
		AS_IF([test "x$enable_curl" != "xyes"],
			[CURLCONFIG="$enable_curl/bin/curl-config"])

		AC_MSG_CHECKING([$CURLCONFIG])
		AS_IF([$CURLCONFIG --version 2>/dev/null >/dev/null],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_RESULT([no])
				AC_MSG_FAILURE([Could not find curl-config, try editing --enable-libcurl])])

		CURLCFLAG="`$CURLCONFIG --cflags`"
		CURLLIBS="`$CURLCONFIG --libs`"

		dnl This test must be this way because of #3981
		AS_IF([$CURLCONFIG --features | grep -q -e AsynchDNS],
			[CURLUSESCARES="1"],
			[CURLUSESCARES="0"])
		AS_IF([test "$CURLUSESCARES" = "0"],
			[AC_MSG_WARN([cURL seems compiled without c-ares support. Your IRCd will possibly stall when REHASHing!])])

		dnl sanity warnings
		AS_IF([test -z "${CURLLIBS}"],
			[AC_MSG_WARN([CURLLIBS is empty, that probably means that I could not find $enable_curl/bin/curl-config])])

		dnl Ok this is ugly, basically we need to strip the version of c-ares that curl uses
		dnl because we want to use our own version (which is hopefully fully binary
		dnl compatible with the curl one as well).
		dnl Therefore we need to strip the cares libs in a weird way...
		dnl If anyone can come up with something better and still portable (no awk!?)
		dnl then let us know. -- Syzop
		dnl
		dnl It is dangerous to mix and match cURL with potentially ABI-incompatible versions of
		dnl c-ares, just use --with-system-cares.
		dnl Thus, make sure to use --with-system-cares when using system-cURL. If the user
		dnl wants bundled c-ares + system libcURL, then we should filter out c-ares
		dnl flags. _Only_ in that case should we mess with the flags. -- ohnobinki

		AS_IF([test "x$with_system_cares" = "xno" && test "x$HOME/curl" != "x$enable_curl" && test "x/usr/share/unreal-curl" != "x$enable_curl" && test "$CURLUSESCARES" != "0" ],
		[
			AC_MSG_ERROR([[

  You have decided to build unrealIRCd with libcURL (remote includes) support.
  However, you have disabled system-installed c-ares support (--with-system-cares).
  Because UnrealIRCd will use a bundled copy of c-ares which may be incompatible
  with the system-installed libcURL, this is a bad idea which may result in error
  messages looking like:

  	\`\`[error] unrealircd.conf:9: include: error downloading '(http://example.net/ex.conf)': Could not resolve host: example.net (Successful completion)''

  Or UnrealIRCd might even crash.

  Please build UnrealIRCd with --with-system-cares when enabling --enable-libcurl
]])
		])

		AS_IF([test "x`echo $CURLLIBS |grep ares`" != x && test "x$with_system_cares" = "xno"],
		[
			dnl Attempt one: Linux sed
			[XCURLLIBS="`echo "$CURLLIBS"|sed -r 's/[^ ]*ares[^ ]*//g' 2>/dev/null`"]
			AS_IF([test "x$XCURLLIBS" = "x"],
			[
				dnl Attempt two: FreeBSD (and others?) sed
				[XCURLLIBS="`echo "$CURLLIBS"|sed -E 's/[^ ]*ares[^ ]*//g' 2>/dev/null`"]
				AS_IF([test x"$XCURLLIBS" = x],
				[
					AC_MSG_ERROR([sed appears to be broken. It is needed for a remote includes compile hack.])
				])
			])
			CURLLIBS="$XCURLLIBS"

			IRCDLIBS_CURL_CARES="$CARES_LIBS"
			CFLAGS_CURL_CARES="$CARES_CFLAGS"
		])
		
		dnl Make sure that linking against cURL works rather than letting the user
		dnl find out after compiling most of his program. ~ohnobinki
		IRCDLIBS="$IRCDLIBS $CURLLIBS"
		CFLAGS="$CFLAGS $CURLCFLAG"
		AC_DEFINE([USE_LIBCURL], [], [Define if you have libcurl installed to get remote includes and MOTD support])

		AC_MSG_CHECKING([curl_easy_init() in $CURLLIBS])
		LIBS_SAVEDA="$LIBS"
		CFLAGS_SAVEDA="$CFLAGS"

		LIBS="$IRCDLIBS $IRCDLIBS_CURL_CARES"
		CFLAGS="$CFLAGS $CFLAGS_CURL_CARES"
		AC_LINK_IFELSE(
		    [
			AC_LANG_PROGRAM(
			    [[#include <curl/curl.h>]],
			    [[CURL *curl = curl_easy_init();]])
			],
		    [AC_MSG_RESULT([yes])],
		    [AC_MSG_RESULT([no])
			AC_MSG_FAILURE([You asked for libcURL (remote includes) support, but it can't be found at $enable_curl])
		])
		LIBS="$LIBS_SAVEDA"
		CFLAGS="$CFLAGS_SAVEDA"

		URL="url.o"
		AC_SUBST(URL)
	]) dnl AS_IF(enable_curl) 
])

dnl the following 2 macros are based on CHECK_SSL by Mark Ethan Trostler <trostler@juniper.net> 

AC_DEFUN([CHECK_SSL],
[
AC_ARG_ENABLE(ssl,
	[AC_HELP_STRING([--enable-ssl=],[enable ssl will check /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr])],
	[],
	[enable_ssl=no])
AS_IF([test $enable_ssl != "no"],
	[ 
	AC_MSG_CHECKING([for openssl])
	for dir in $enable_ssl /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr; do
		ssldir="$dir"
		if test -f "$dir/include/openssl/ssl.h"; then
			AC_MSG_RESULT([found in $ssldir/include/openssl])
			found_ssl="yes";
			if test ! "$ssldir" = "/usr" ; then
				CFLAGS="$CFLAGS -I$ssldir/include";
			fi
			break
		fi
		if test -f "$dir/include/ssl.h"; then
			AC_MSG_RESULT([found in $ssldir/include])
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
		echo "Please install the needed binaries and libraries."
		echo "The package is often called 'openssl-dev', 'openssl-devel' or 'libssl-dev'"
		echo "After doing so re-run ./Config"
		exit 1
	else
		CRYPTOLIB="-lssl -lcrypto";
		if test ! "$ssldir" = "/usr" ; then
			LDFLAGS="$LDFLAGS -L$ssldir/lib";
		fi
	fi
	])
])
