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
		AS_IF([$CURLCONFIG --libs | grep -q -e ares],
			[CURLUSESCARES="1"],
			[CURLUSESCARES="0"])

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

		AS_IF([test "x$has_system_cares" = "xno" && test "x$BUILDDIR/extras/curl" != "x$enable_curl" && test "$CURLUSESCARES" != "0" ],
		[
			AC_MSG_ERROR([[

  You have decided to build unrealIRCd with libcURL (remote includes) support.
  However, you have system-installed c-ares support has either been disabled
  (--without-system-cares) or is unavailable.
  Because UnrealIRCd will use a bundled copy of c-ares which may be incompatible
  with the system-installed libcURL, this is a bad idea which may result in error
  messages looking like:

  	error downloading ... Could not resolve host: example.net (Successful completion)

  Or UnrealIRCd might even crash.

  Please build UnrealIRCd with --with-system-cares when enabling --enable-libcurl
]])
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

		dnl Finally, choose the cURL implementation of url.c
		URL="url_curl.o"
	],[
		dnl Choose UnrealIRCds internal implementation of url.c
		URL="url_unreal.o"
	]) dnl AS_IF(enable_curl) 
	AC_SUBST(URL)
])

dnl the following 2 macros are based on CHECK_SSL by Mark Ethan Trostler <trostler@juniper.net> 

AC_DEFUN([CHECK_SSL],
[
AC_ARG_ENABLE(ssl,
	[AC_HELP_STRING([--enable-ssl=],[enable ssl will check /usr/local/opt/openssl /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr])],
	[],
	[enable_ssl=no])
AS_IF([test $enable_ssl != "no"],
	[ 
	AC_MSG_CHECKING([for OpenSSL])
	for dir in $enable_ssl /usr/local/opt/openssl /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr; do
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
		echo "The following packages are required:"
		echo "1) The library package is often called 'openssl-dev', 'openssl-devel' or 'libssl-dev'"
		echo "2) The binary package is usually called 'openssl'."
		echo "NOTE: you or your system administrator needs to install the library AND the binary package."
		echo "After doing so, simply re-run ./Config"
		exit 1
	else
		CRYPTOLIB="-lssl -lcrypto";
		if test ! "$ssldir" = "/usr" ; then
			if test -d "$ssldir/lib64" ; then
				LDFLAGS="$LDFLAGS -L$ssldir/lib64";
			else
				LDFLAGS="$LDFLAGS -L$ssldir/lib";
			fi
			dnl check if binary path exists
			if test -f "$ssldir/bin/openssl"; then
			    OPENSSLPATH="$ssldir/bin/openssl";
			fi
		fi
		dnl linking require -ldl?
		AC_MSG_CHECKING([OpenSSL linking with -ldl])
		SAVE_LIBS="$LIBS"
		LIBS="$LIBS $CRYPTOLIB -ldl"
		AC_TRY_LINK([#include <openssl/err.h>], [ERR_clear_error();],
		[
			AC_MSG_RESULT(yes)
			CRYPTOLIB="$CRYPTOLIB -ldl"
		],
		[
			AC_MSG_RESULT(no)

			dnl linking require both -ldl and -lpthread?
			AC_MSG_CHECKING([OpenSSL linking with -ldl and -lpthread])
			LIBS="$SAVE_LIBS $CRYPTOLIB -ldl -lpthread"
			AC_TRY_LINK([#include <openssl/err.h>], [ERR_clear_error();],
			[
				AC_MSG_RESULT(yes)
				CRYPTOLIB="$CRYPTOLIB -ldl -lpthread"
			],
			[
				AC_MSG_RESULT(no)
			])
		])
		LIBS="$SAVE_LIBS"
	fi
	])
])

AC_DEFUN([CHECK_SSL_CTX_SET1_CURVES_LIST],
[
AC_MSG_CHECKING([for SSL_CTX_set1_curves_list in SSL library])
AC_LANG_PUSH(C)
SAVE_LIBS="$LIBS"
LIBS="$LIBS $CRYPTOLIB"
AC_TRY_LINK([#include <openssl/ssl.h>],
	[SSL_CTX *ctx = NULL; SSL_CTX_set1_curves_list(ctx, "test");],
	has_function=1,
	has_function=0)
LIBS="$SAVE_LIBS"
AC_LANG_POP(C)
if test $has_function = 1; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAS_SSL_CTX_SET1_CURVES_LIST], [], [Define if ssl library has SSL_CTX_set1_curves_list])
else
	AC_MSG_RESULT([no])
fi
])

AC_DEFUN([CHECK_SSL_CTX_SET_MIN_PROTO_VERSION],
[
AC_MSG_CHECKING([for SSL_CTX_set_min_proto_version in SSL library])
AC_LANG_PUSH(C)
SAVE_LIBS="$LIBS"
LIBS="$LIBS $CRYPTOLIB"
AC_TRY_LINK([#include <openssl/ssl.h>],
	[SSL_CTX *ctx = NULL; SSL_CTX_set_min_proto_version(ctx, TLS1_VERSION);],
	has_function=1,
	has_function=0)
LIBS="$SAVE_LIBS"
AC_LANG_POP(C)
if test $has_function = 1; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAS_SSL_CTX_SET_MIN_PROTO_VERSION], [], [Define if ssl library has SSL_CTX_set_min_proto_version])
else
	AC_MSG_RESULT([no])
fi
])

AC_DEFUN([CHECK_SSL_CTX_SET_SECURITY_LEVEL],
[
AC_MSG_CHECKING([for SSL_CTX_set_security_level in SSL library])
AC_LANG_PUSH(C)
SAVE_LIBS="$LIBS"
LIBS="$LIBS $CRYPTOLIB"
AC_TRY_LINK([#include <openssl/ssl.h>],
	[SSL_CTX *ctx = NULL; SSL_CTX_set_security_level(ctx, 1);],
	has_function=1,
	has_function=0)
LIBS="$SAVE_LIBS"
AC_LANG_POP(C)
if test $has_function = 1; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAS_SSL_CTX_SET_SECURITY_LEVEL], [], [Define if ssl library has SSL_CTX_set_security_level])
else
	AC_MSG_RESULT([no])
fi
])

AC_DEFUN([CHECK_ASN1_TIME_diff],
[
AC_MSG_CHECKING([for ASN1_TIME_diff in SSL library])
AC_LANG_PUSH(C)
SAVE_LIBS="$LIBS"
LIBS="$LIBS $CRYPTOLIB"
AC_TRY_LINK([#include <openssl/ssl.h>],
	[int one, two; ASN1_TIME_diff(&one, &two, NULL, NULL);],
	has_function=1,
	has_function=0)
LIBS="$SAVE_LIBS"
AC_LANG_POP(C)
if test $has_function = 1; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAS_ASN1_TIME_diff], [], [Define if ssl library has ASN1_TIME_diff])
else
	AC_MSG_RESULT([no])
fi
])

AC_DEFUN([CHECK_X509_get0_notAfter],
[
AC_MSG_CHECKING([for X509_get0_notAfter in SSL library])
AC_LANG_PUSH(C)
SAVE_LIBS="$LIBS"
LIBS="$LIBS $CRYPTOLIB"
AC_TRY_LINK([#include <openssl/ssl.h>],
	[X509_get0_notAfter(NULL);],
	has_function=1,
	has_function=0)
LIBS="$SAVE_LIBS"
AC_LANG_POP(C)
if test $has_function = 1; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAS_X509_get0_notAfter], [], [Define if ssl library has X509_get0_notAfter])
else
	AC_MSG_RESULT([no])
fi
])

AC_DEFUN([CHECK_X509_check_host],
[
AC_MSG_CHECKING([for X509_check_host in SSL library])
AC_LANG_PUSH(C)
SAVE_LIBS="$LIBS"
LIBS="$LIBS $CRYPTOLIB"
AC_TRY_LINK([#include <openssl/x509v3.h>],
	[X509_check_host(NULL, NULL, 0, 0, NULL);],
	has_function=1,
	has_function=0)
LIBS="$SAVE_LIBS"
AC_LANG_POP(C)
if test $has_function = 1; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAS_X509_check_host], [], [Define if ssl library has X509_check_host])
else
	AC_MSG_RESULT([no])
fi
])

dnl For geoip-api-c
AC_DEFUN([CHECK_GEOIP_CLASSIC],
[
	AC_ARG_ENABLE(geoip_classic,
	[AC_HELP_STRING([--enable-geoip-classic=no/yes],[enable GeoIP Classic support])],
	[enable_geoip_classic=$enableval],
	[enable_geoip_classic=no])

	AS_IF([test "x$enable_geoip_classic" = "xyes"],
	[
		dnl First see if the system provides it
		has_system_geoip_classic="no"
		PKG_CHECK_MODULES([GEOIP_CLASSIC], [geoip >= 1.6.0],
		                  [has_system_geoip_classic=yes
		                   AS_IF([test "x$PRIVATELIBDIR" != "x"], [rm -f "$PRIVATELIBDIR/"libGeoIP.*])],
		                  [has_system_geoip_classic=no])

		dnl Otherwise fallback to our own..
		AS_IF([test "$has_system_geoip_classic" = "no"],[
			dnl REMEMBER TO CHANGE WITH A NEW GEOIP LIBRARY RELEASE!
			geoip_classic_version="1.6.12"
			AC_MSG_RESULT(extracting GeoIP Classic library)
			cur_dir=`pwd`
			cd extras
			dnl remove old directory to force a recompile...
			dnl and remove its installation prefix just to clean things up.
			rm -rf GeoIP-$geoip_classic_version geoip-classic
			if test "x$ac_cv_path_GUNZIP" = "x" ; then
				tar xfz geoip-classic.tar.gz
			else
				cp geoip-classic.tar.gz geoip-classic.tar.gz.bak
				gunzip -f geoip-classic.tar.gz
				cp geoip-classic.tar.gz.bak geoip-classic.tar.gz
				tar xf geoip-classic.tar
			fi
			AC_MSG_RESULT(configuring GeoIP Classic library)
			cd GeoIP-$geoip_classic_version
			save_cflags="$CFLAGS"
			CFLAGS="$orig_cflags"
			export CFLAGS
			./configure --prefix=$cur_dir/extras/geoip-classic --libdir=$PRIVATELIBDIR --enable-shared --disable-static || exit 1
			CFLAGS="$save_cflags"
			AC_MSG_RESULT(compiling GeoIP Classic library)
			$ac_cv_prog_MAKER || exit 1
			AC_MSG_RESULT(installing GeoIP Classic library)
			$ac_cv_prog_MAKER install || exit 1
			dnl Try pkg-config first...
			AS_IF([test -n "$ac_cv_path_PKGCONFIG"],
			       [GEOIP_CLASSIC_LIBS="`$ac_cv_path_PKGCONFIG --libs geoip.pc`"
			        GEOIP_CLASSIC_CFLAGS="`$ac_cv_path_PKGCONFIG --cflags geoip.pc`"])
			dnl In case the system does not have pkg-config, fallback to hardcoded settings...
			AS_IF([test -z "$GEOIP_CLASSIC_LIBS"],
			       [GEOIP_CLASSIC_LIBS="-L$PRIVATELIBDIR -lGeoIP"
			        GEOIP_CLASSIC_CFLAGS="-I$cur_dir/extras/geoip-classic/include"])
			cd $cur_dir
		])
		
		AC_SUBST(GEOIP_CLASSIC_LIBS)
		AC_SUBST(GEOIP_CLASSIC_CFLAGS)

		GEOIP_CLASSIC_OBJECTS="geoip_classic.so"
		AC_SUBST(GEOIP_CLASSIC_OBJECTS)
	]) dnl AS_IF(enable_geoip_classic) 
])

AC_DEFUN([CHECK_LIBMAXMINDDB],
[
	AC_ARG_ENABLE(libmaxminddb,
	[AC_HELP_STRING([--enable-libmaxminddb=no/yes],[enable GeoIP libmaxminddb support])],
	[enable_libmaxminddb=$enableval],
	[enable_libmaxminddb=no])

	AS_IF([test "x$enable_libmaxminddb" = "xyes"],
	[
		dnl see if the system provides it
		has_system_libmaxminddb="no"
		PKG_CHECK_MODULES([LIBMAXMINDDB], [libmaxminddb >= 1.4.3],
		                  [has_system_libmaxminddb=yes])
		AS_IF([test "x$has_system_libmaxminddb" = "xyes"],
		[

			AC_SUBST(LIBMAXMINDDB_LIBS)
			AC_SUBST(LIBMAXMINDDB_CFLAGS)

			GEOIP_MAXMIND_OBJECTS="geoip_maxmind.so"
			AC_SUBST(GEOIP_MAXMIND_OBJECTS)
		])
	])
])

