dnl Process this file with autoconf to produce a configure script.

dnl When updating the version, remember to update the following files
dnl appropriately:
dnl include/windows/setup.h
dnl src/windows/unrealinst.iss
dnl doc/Config.header
dnl src/version.c.SH

AC_INIT([unrealircd], [6.0.3], [https://bugs.unrealircd.org/], [], [https://unrealircd.org/])
AC_CONFIG_SRCDIR([src/ircd.c])
AC_CONFIG_HEADER([include/setup.h])
AC_CONFIG_AUX_DIR([autoconf])
AC_CONFIG_MACRO_DIR([autoconf/m4])

if test "x$enable_dynamic_linking" = "x"; then
	echo "Please use ./Config instead of ./configure"
	exit 1
fi

dnl Save CFLAGS, use this when building the libraries like c-ares
orig_cflags="$CFLAGS"

dnl Save build directory early on (used in our m4 macros too)
BUILDDIR_NOW="`pwd`"

dnl Calculate the versions. Perhaps the use of expr is a little too extravagant
# Generation version number (e.g.: X in X.Y.Z)
UNREAL_VERSION_GENERATION=["6"]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_GENERATION], [$UNREAL_VERSION_GENERATION], [Generation version number (e.g.: X for X.Y.Z)])

# Major version number (e.g.: Y in X.Y.Z)
UNREAL_VERSION_MAJOR=["0"]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_MAJOR], [$UNREAL_VERSION_MAJOR], [Major version number (e.g.: Y for X.Y.Z)])

# Minor version number (e.g.: Z in X.Y.Z)
UNREAL_VERSION_MINOR=["3"]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_MINOR], [$UNREAL_VERSION_MINOR], [Minor version number (e.g.: Z for X.Y.Z)])

# The version suffix such as a beta marker or release candidate
# marker. (e.g.: -rcX for unrealircd-3.2.9-rcX). This macro is a
# string instead of an integer because it contains arbitrary data.
UNREAL_VERSION_SUFFIX=[""]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_SUFFIX], ["$UNREAL_VERSION_SUFFIX"], [Version suffix such as a beta marker or release candidate marker. (e.g.: -rcX for unrealircd-3.2.9-rcX)])

AC_PATH_PROG(RM,rm)
AC_PATH_PROG(CP,cp)
AC_PATH_PROG(TOUCH,touch)
AC_PATH_PROG(OPENSSLPATH,openssl)
AS_IF([test x"$OPENSSLPATH" = "x"],
[
echo ""
echo "Apparently you do not have both the openssl binary and openssl development libraries installed."
echo "The following packages are required:"
echo "1) The library package is often called 'openssl-dev', 'openssl-devel' or 'libssl-dev'"
echo "2) The binary package is usually called 'openssl'."
echo "NOTE: you or your system administrator needs to install the library AND the binary package."
echo "After doing so, simply re-run ./Config"
exit 1
])

AC_PATH_PROG(INSTALL,install)
AC_PATH_PROG(GUNZIP, gunzip)
AC_PATH_PROG(PKGCONFIG, pkg-config)

dnl Check for compiler
AC_PROG_CC_C99
AS_IF([test "$ac_cv_prog_cc_c99" = "no"],
	[AC_MSG_ERROR([No C99 compiler was found. Please install gcc or clang and other build tools. Eg, on Debian/Ubuntu you probably want to run the following as root: apt-get install build-essential ])])

dnl Check for make moved down, so the above compiler check takes precedence.
AC_CHECK_PROG(MAKER, gmake, gmake, make)
AC_PATH_PROG(GMAKE,gmake)
AS_IF([$MAKER --version | grep -q "GNU Make"],
        [GNUMAKE="0"],
        [AC_MSG_ERROR([It seems your system does not have make/gmake installed. If you are on Linux then install make, otherwise install gmake.])])

dnl Checks for libraries.
AC_CHECK_LIB(descrypt, crypt,
	[AC_DEFINE([HAVE_CRYPT], [], [Define if you have crypt])
		IRCDLIBS="$IRCDLIBS-ldescrypt "],
	[AC_CHECK_LIB(crypt, crypt,
		[AC_DEFINE([HAVE_CRYPT], [], [Define if you have crypt])
			IRCDLIBS="$IRCDLIBS-lcrypt "])])

dnl Check for big-endian system, even though these hardly exist anymore...
AS_CASE([$host_cpu],
  [i?86|amd64|x86_64],
    [ac_cv_c_bigendian=no]
)
AC_C_BIGENDIAN(
  AC_DEFINE(NATIVE_BIG_ENDIAN, 1, [machine is bigendian]),
  AC_DEFINE(NATIVE_LITTLE_ENDIAN, 1, [machine is littleendian]),
  AC_MSG_ERROR([unknown endianness]),
  AC_MSG_ERROR([universal endianness is not supported - compile separately and use lipo(1)])
)

dnl HARDENING START
dnl This is taken from https://github.com/kmcallister/autoharden
dnl With some very small modifications (to remove C++ checking for instance)
# We want to check for compiler flag support, but there is no way to make
# clang's "argument unused" warning fatal.  So we invoke the compiler through a
# wrapper script that greps for this message.
saved_CC="$CC"
saved_CXX="$CXX"
saved_LD="$LD"
flag_wrap="$srcdir/extras/wrap-compiler-for-flag-check"
CC="$flag_wrap $CC"
CXX="$flag_wrap $CXX"
LD="$flag_wrap $LD"

# We use the same hardening flags for C and C++.  We must check that each flag
# is supported by both compilers.
AC_DEFUN([check_cc_flag],
 [AC_LANG_PUSH(C)
  AX_CHECK_COMPILE_FLAG([$1], [$2], [$3], [-Werror $4])
  AC_LANG_POP(C)])

AC_DEFUN([check_link_flag],
 [AX_CHECK_LINK_FLAG([$1], [$2], [$3], [-Werror $4])])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--enable-hardening],
    [Enable compiler and linker options to frustrate memory corruption exploits @<:@yes@:>@])],
  [hardening="$enableval"],
  [hardening="yes"])

HARDEN_CFLAGS=""
HARDEN_LDFLAGS=""
AS_IF([test x"$hardening" != x"no"], [
  check_cc_flag([-fno-strict-overflow], [HARDEN_CFLAGS="$HARDEN_CFLAGS -fno-strict-overflow"])

  # This one will likely succeed, even on platforms where it does nothing.
  check_cc_flag([-D_FORTIFY_SOURCE=2], [HARDEN_CFLAGS="$HARDEN_CFLAGS -D_FORTIFY_SOURCE=2"])

  check_cc_flag([-fstack-protector-all],
   [check_link_flag([-fstack-protector-all],
     [HARDEN_CFLAGS="$HARDEN_CFLAGS -fstack-protector-all"
      check_cc_flag([-Wstack-protector], [HARDEN_CFLAGS="$HARDEN_CFLAGS -Wstack-protector"],
        [], [-fstack-protector-all])
      check_cc_flag([--param ssp-buffer-size=1], [HARDEN_CFLAGS="$HARDEN_CFLAGS --param ssp-buffer-size=1"],
        [], [-fstack-protector-all])])])

  # Added in UnrealIRCd 5.0.5 (default on Ubuntu 19.10)
  check_cc_flag([-fstack-clash-protection], [HARDEN_CFLAGS="$HARDEN_CFLAGS -fstack-clash-protection"])

  # Control Flow Enforcement (ROP hardening) - requires CPU hardware support
  check_cc_flag([-fcf-protection], [HARDEN_CFLAGS="$HARDEN_CFLAGS -fcf-protection"])

  # At the link step, we might want -pie (GCC) or -Wl,-pie (Clang on OS X)
  #
  # The linker checks also compile code, so we need to include -fPIE as well.
  check_cc_flag([-fPIE],
   [check_link_flag([-fPIE -pie],
     [HARDEN_BINCFLAGS="-fPIE"
      HARDEN_BINLDFLAGS="-pie"],
     [check_link_flag([-fPIE -Wl,-pie],
       [HARDEN_BINCFLAGS="-fPIE"
        HARDEN_BINLDFLAGS="-Wl,-pie"])])])

  check_link_flag([-Wl,-z,relro],
   [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,relro"
    check_link_flag([-Wl,-z,now], [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,now"])])])
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])
AC_SUBST([HARDEN_BINCFLAGS])
AC_SUBST([HARDEN_BINLDFLAGS])

# End of flag tests.
CC="$saved_CC"
CXX="$saved_CXX"
LD="$saved_LD"
dnl HARDENING END

dnl UnrealIRCd might not be strict-aliasing safe at this time
check_cc_flag([-fno-strict-aliasing], [CFLAGS="$CFLAGS -fno-strict-aliasing"])

dnl UnrealIRCd should be able to compile with -fno-common
dnl This also makes ASan (if it is in use) able to instrument these variables.
check_cc_flag([-fno-common], [CFLAGS="$CFLAGS -fno-common"])

dnl Previously -funsigned-char was in a config check. It would always
dnl be enabled with gcc and clang. We now unconditionally enable it,
dnl skipping the check. This will cause an error if someone uses a
dnl non-gcc/non-clang compiler that does not support -funsigned-char
dnl which is good. After all, we really depend on it.
dnl UnrealIRCd should never be compiled without char being unsigned.
CFLAGS="$CFLAGS -funsigned-char"

dnl Compiler -W checks...

dnl == ADD THESE WARNINGS ==

dnl We should be able to turn this on unconditionally:
CFLAGS="$CFLAGS -Wall"

dnl More warnings (if the compiler supports it):
check_cc_flag([-Wextra], [CFLAGS="$CFLAGS -Wextra"])
check_cc_flag([-Waggregate-return], [CFLAGS="$CFLAGS -Waggregate-return"])
check_cc_flag([-Wformat-nonliteral], [CFLAGS="$CFLAGS -Wformat-nonliteral"])

dnl The following few are more experimental, if they have false positives we'll have
dnl to disable them:
dnl Can't use this, too bad: check_cc_flag([-Wlogical-op], [CFLAGS="$CFLAGS -Wlogical-op"])
check_cc_flag([-Wduplicated-cond], [CFLAGS="$CFLAGS -Wduplicated-cond"])
check_cc_flag([-Wduplicated-branches], [CFLAGS="$CFLAGS -Wduplicated-branches"])

check_cc_flag([-Wparentheses], [CFLAGS="$CFLAGS -Wparentheses"])

dnl == REMOVE THESE WARNINGS ==

dnl And now to filter out certain warnings:
dnl [!] NOTE REGARDING THE check_cc_flag used by these:
dnl We check for the -Woption even though we are going to use -Wno-option.
dnl This is due to the following (odd) gcc behavior:
dnl "When an unrecognized warning option is requested (e.g.,
dnl  -Wunknown-warning), GCC emits a diagnostic stating that the option is not
dnl  recognized.  However, if the -Wno- form is used, the behavior is slightly
dnl  different: no diagnostic is produced for -Wno-unknown-warning unless
dnl  other diagnostics are being produced.  This allows the use of new -Wno-
dnl  options with old compilers, but if something goes wrong, the compiler
dnl  warns that an unrecognized option is present."
dnl Since we don't want to use any unrecognized -Wno-option, we test for
dnl -Woption instead.

dnl Pointer signedness warnings are really a pain and 99.9% of the time
dnl they are of absolutely no use whatsoever. IMO the person who decided
dnl to enable this without -Wall should be shot on sight.
check_cc_flag([-Wpointer-sign], [CFLAGS="$CFLAGS -Wno-pointer-sign"])

dnl This is purely for charsys.c... I like it so we can easily read
dnl this for non-utf8. We can remove it once we ditch non-utf8 some day
dnl of course, or decide to ignore me and encode them.
check_cc_flag([-Winvalid-source-encoding], [CFLAGS="$CFLAGS -Wno-invalid-source-encoding"])

check_cc_flag([-Wformat-zero-length], [CFLAGS="$CFLAGS -Wno-format-zero-length"])

check_cc_flag([-Wformat-truncation], [CFLAGS="$CFLAGS -Wno-format-truncation"])

dnl While it can be useful to occasionally to compile with warnings about
dnl unused variables and parameters, we often 'think ahead' when coding things
dnl so they may be useless now but not later. Similarly, for variables, we
dnl don't always care about a variable that may still be present in a build
dnl without DEBUGMODE. Unused variables are optimized out anyway.
check_cc_flag([-Wunused], [CFLAGS="$CFLAGS -Wno-unused"])
check_cc_flag([-Wunused-parameter], [CFLAGS="$CFLAGS -Wno-unused-parameter"])
check_cc_flag([-Wunused-but-set-parameter], [CFLAGS="$CFLAGS -Wno-unused-but-set-parameter"])

dnl We use this and this warning is meaningless since 'char' is always unsigned
dnl in UnrealIRCd compiles (-funsigned-char).
check_cc_flag([-Wchar-subscripts], [CFLAGS="$CFLAGS -Wno-char-subscripts"])

check_cc_flag([-Wsign-compare], [CFLAGS="$CFLAGS -Wno-sign-compare"])

dnl Don't warn about empty body, we use this, eg via Debug(()) or in if's.
check_cc_flag([-Wempty-body], [CFLAGS="$CFLAGS -Wno-empty-body"])

dnl Yeah this old clang version is a bit problematic
dnl (ships in Ubuntu 16.04 for example)
dnl -Wtautological-compare has false positives
dnl -Wno-pragmas is needed, despite -Wno-unknown-warning-option
AS_IF([$CC --version | grep -q "clang version 3."],
        [CFLAGS="$CFLAGS -Wno-tautological-compare -Wno-pragmas"])

dnl This one MUST be LAST!!
dnl It disables -Wsomeunknownoption being an error. Which is needed for
dnl the pragma's in individual files to selectively disable some warnings
dnl on clang/gcc (that may exist in eg gcc but not in clang or vice versa).
check_cc_flag([-Wpragmas], [no_pragmas=1],[no_pragmas=0])
check_cc_flag([-Wunknown-warning-option], [unknown_warning_option=1], [unknown_warning_option=0])

if test "$unknown_warning_option" = "1"; then
	dnl This is the best option
	CFLAGS="$CFLAGS -Wno-unknown-warning-option"
else
	if test "$no_pragmas" = "1"; then
		dnl This is a fallback needed for older gcc/clang, it also
		dnl disables several other useful warnings/errors related
		dnl to pragma's unfortunately.
		CFLAGS="$CFLAGS -Wno-pragmas"
	fi
fi

dnl End of -W... compiler checks.


dnl module checking based on Hyb7's module checking code
AC_DEFUN([AC_ENABLE_DYN],
[
AC_CHECK_FUNC(dlopen,, [AC_CHECK_LIB(dl, dlopen,IRCDLIBS="$IRCDLIBS -ldl",
[
AC_MSG_WARN(Dynamic linking is not enabled because dlopen was not found) 
AC_DEFINE(STATIC_LINKING)
])])

hold_cflags=$CFLAGS
DYNAMIC_LDFLAGS=""
CFLAGS="$CFLAGS -Wl,-export-dynamic"
AC_CACHE_CHECK(if we need the -export-dynamic flag, ac_cv_export_dynamic, [
AC_TRY_LINK(, [int i];, ac_cv_export_dynamic=yes, ac_cv_export_dynamic=no)])
CFLAGS=$hold_cflags
if test "$ac_cv_export_dynamic" = "yes"; then
	DYNAMIC_LDFLAGS="-Wl,-export-dynamic"
fi

AC_CACHE_CHECK(for compiler option to produce PIC,ac_cv_pic,[
if test "$ac_cv_prog_gcc" = "yes"; then
	ac_cv_pic="-fPIC -DPIC -shared"
	case `uname -s` in
		Darwin*[)]
		ac_cv_pic="-std=gnu89 -bundle -flat_namespace -undefined suppress"
		;;
		HP-UX*[)]
		ac_cv_pic="-fPIC"
		;;
	esac
else
case `uname -s` in
	SunOS*[)]
	ac_cv_pic="-KPIC -DPIC -G"
	;;
esac
fi
])
AC_CACHE_CHECK(if your system prepends an underscore on symbols,ac_cv_underscore,[
cat >uscore.c << __EOF__
int main() {
	return 0;
}
__EOF__
$CC -o uscore $CFLAGS uscore.c 1>&5
if test -z "`strings -a uscore |grep '^_main$'`"; then
ac_cv_underscore=no
else
ac_cv_underscore=yes
fi
rm -f uscore uscore.c
])
dnl libtool has built-in tests that determine proper underscorage
if test "$ac_cv_underscore" = "yes"; then
	AC_DEFINE([UNDERSCORE], [], [Define if your system prepends an underscore to symbols])
fi

MODULEFLAGS="$ac_cv_pic $DYNAMIC_LDFLAGS"
dnl DYNAMIC_LINKING is not meant to be defined in include/setup.h, it's 
dnl defined in the Makefiles using -D. Having it defined globally will
dnl only cause braindamage and symbol collisions :-D.
dnl AC_DEFINE([DYNAMIC_LINKING], [], [Link dynamically as opposed to statically. (Dynamic linking is the only supported method of linking atm)])
])

AC_CACHE_CHECK([if your system has IPv6 support], [ac_cv_ip6], [
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
int main() {
int s = socket(AF_INET6, SOCK_STREAM, 0);
exit(0); /* We only check if the code compiles, that's enough. We can deal with missing runtime IPv6 */
}
],
	[ac_cv_ip6=yes],
	[ac_cv_ip6=no])
])
if test "$ac_cv_ip6" = "no"; then
	AC_MSG_ERROR([Your system does not support IPv6])
fi

AC_CHECK_HEADER(sys/syslog.h,
	AC_DEFINE([SYSSYSLOGH], [], [Define if you have the <sys/syslog.h> header file.]))
AC_CHECK_HEADER(sys/rusage.h,
	AC_DEFINE([RUSAGEH], [], [Define if you have the <sys/rusage.h> header file.]))
AC_CHECK_HEADER(glob.h,
	AC_DEFINE([GLOBH], [], [Define if you have the <glob.h> header file.]))
AC_CHECK_HEADERS([stdint.h inttypes.h])

dnl Checks for library functions.
AC_CHECK_FUNCS(strlcpy,
	AC_DEFINE([HAVE_STRLCPY], [], [Define if you have strlcpy. Otherwise, an internal implementation will be used!]))
AC_CHECK_FUNCS(strlcat,
	AC_DEFINE([HAVE_STRLCAT], [], [Define if you have strlcat]))
AC_CHECK_FUNCS(strlncat,
	AC_DEFINE([HAVE_STRLNCAT], [], [Define if you have strlncat]))
AC_CHECK_FUNCS(strlncpy,
	AC_DEFINE([HAVE_STRLNCPY], [], [Define if you have strlncpy]))

AC_CHECK_FUNCS([getrusage],
	[AC_DEFINE([GETRUSAGE_2], [], [Define if you have getrusage])],
	[AC_CHECK_FUNCS([times],
		[AC_DEFINE([TIMES_2], [], [Define if you have times])])])
AC_CHECK_FUNCS([setproctitle],
	[AC_DEFINE([HAVE_SETPROCTITLE], [], [Define if you have setproctitle])],
	[AC_CHECK_LIB([util],
		[setproctitle],
		[AC_DEFINE([HAVE_SETPROCTITLE], [], [Define if you have setproctitle])
		IRCDLIBS="$IRCDLIBS-lutil"],
		[
		AC_EGREP_HEADER([#define.*PS_STRINGS.*],[sys/exec.h],
			[AC_DEFINE([HAVE_PSSTRINGS],[], [Define if you have PS_STRINGS])],
			[AC_CHECK_FUNCS([pstat],
				[AC_DEFINE([HAVE_PSTAT], [], [Define if you have pstat])])])
		])
	]
)

AC_CHECK_FUNCS(explicit_bzero,AC_DEFINE([HAVE_EXPLICIT_BZERO], [], [Define if you have explicit_bzero]))
AC_CHECK_FUNCS(syslog,AC_DEFINE([HAVE_SYSLOG], [], [Define if you have syslog]))
AC_SUBST(CRYPTOLIB)
AC_SUBST(MODULEFLAGS)
AC_SUBST(DYNAMIC_LDFLAGS)
AC_ARG_WITH(nick-history, [AS_HELP_STRING([--with-nick-history=length],[Specify the length of the nickname history])],
	[AC_DEFINE_UNQUOTED([NICKNAMEHISTORYLENGTH], [$withval], [Set to the nickname history length you want])],
	[AC_DEFINE([NICKNAMEHISTORYLENGTH], [2000], [Set to the nickname history length you want])])
AC_ARG_WITH(permissions, [AS_HELP_STRING([--with-permissions=permissions], [Specify the default permissions for
configuration files])],
	dnl We have an apparently out-of-place 0 here because of a MacOSX bug and because
	dnl we assume that a user thinks that `chmod 0600 blah' is the same as `chmod 600 blah'
	dnl (#3189)
	[AC_DEFINE_UNQUOTED([DEFAULT_PERMISSIONS], [0$withval], [The default permissions for configuration files. Set to 0 to prevent unrealircd from calling chmod() on the files.])],
	[AC_DEFINE([DEFAULT_PERMISSIONS], [0600], [The default permissions for configuration files. Set to 0 to prevent unrealircd from calling chmod() on the files.])]) 

AC_ARG_WITH(bindir, [AS_HELP_STRING([--with-bindir=path],[Specify the directory for the unrealircd binary])],
	[AC_DEFINE_UNQUOTED([BINDIR], ["$withval"], [Define the directory where the unrealircd binary is located])
		BINDIR="$withval"],
	[AC_DEFINE_UNQUOTED([BINDIR], ["$HOME/unrealircd/bin"], [Define the directory where the unrealircd binary is located])
		BINDIR="$HOME/unrealircd/bin"])

AC_ARG_WITH(scriptdir, [AS_HELP_STRING([--with-scriptdir=path],[Specify the directory for the unrealircd start-stop script])],
	[AC_DEFINE_UNQUOTED([SCRIPTDIR], ["$withval"], [Define the directory where the unrealircd start stop scripts is located])
		SCRIPTDIR="$withval"],
	[AC_DEFINE_UNQUOTED([SCRIPTDIR], ["$HOME/unrealircd"], [Define the directory where the unrealircd start stop scripts is located])
		SCRIPTDIR="$HOME/unrealircd"])

AC_ARG_WITH(confdir, [AS_HELP_STRING([--with-confdir=path],[Specify the directory where configuration files are stored])],
	[AC_DEFINE_UNQUOTED([CONFDIR], ["$withval"], [Define the location of the configuration files])
		CONFDIR="$withval"],
	[AC_DEFINE_UNQUOTED([CONFDIR], ["$HOME/unrealircd/conf"], [Define the location of the configuration files])
		CONFDIR="$HOME/unrealircd/conf"])

dnl We have to pass the builddir as well, for the module manager
AC_ARG_WITH(builddir, [AS_HELP_STRING([--with-builddir=path],[Specify the build directory])],
	[AC_DEFINE_UNQUOTED([BUILDDIR], ["$withval"], [Define the build directory])
		BUILDDIR="$withval"],
	[AC_DEFINE_UNQUOTED([BUILDDIR], ["$BUILDDIR_NOW"], [Specify the build directory])
		BUILDDIR="$BUILDDIR_NOW"])

AC_ARG_WITH(modulesdir, [AS_HELP_STRING([--with-modulesdir=path],[Specify the directory for loadable modules])],
	[AC_DEFINE_UNQUOTED([MODULESDIR], ["$withval"], [Define the location of the modules])
		MODULESDIR="$withval"],
	[AC_DEFINE_UNQUOTED([MODULESDIR], ["$HOME/unrealircd/modules"], [Define the location of the modules])
		MODULESDIR="$HOME/unrealircd/modules"])

AC_ARG_WITH(logdir, [AS_HELP_STRING([--with-logdir=path],[Specify the directory where log files are stored])],
	[AC_DEFINE_UNQUOTED([LOGDIR], ["$withval"], [Define the location of the log files])
		LOGDIR="$withval"],
	[AC_DEFINE_UNQUOTED([LOGDIR], ["$HOME/unrealircd/logs"], [Define the location of the log files])
		LOGDIR="$HOME/unrealircd/logs"])

AC_ARG_WITH(cachedir, [AS_HELP_STRING([--with-cachedir=path],[Specify the directory where cached files are stored])],
	[AC_DEFINE_UNQUOTED([CACHEDIR], ["$withval"], [Define the location of the cached remote include files])
		CACHEDIR="$withval"],
	[AC_DEFINE_UNQUOTED([CACHEDIR], ["$HOME/unrealircd/cache"], [Define the location of the cached remote include files])
		CACHEDIR="$HOME/unrealircd/cache"])

AC_ARG_WITH(tmpdir, [AS_HELP_STRING([--with-tmpdir=path],[Specify the directory where private temporary files are stored. Should not be readable or writable by others, so not /tmp!!])],
	[AC_DEFINE_UNQUOTED([TMPDIR], ["$withval"], [Define the location of private temporary files])
		TMPDIR="$withval"],
	[AC_DEFINE_UNQUOTED([TMPDIR], ["$HOME/unrealircd/tmp"], [Define the location of private temporary files])
		TMPDIR="$HOME/unrealircd/tmp"])

AC_ARG_WITH(datadir, [AS_HELP_STRING([--with-datadir=path],[Specify the directory where permanent data is stored])],
	[AC_DEFINE_UNQUOTED([PERMDATADIR], ["$withval"], [Define the location of permanent data files])
		PERMDATADIR="$withval"],
	[AC_DEFINE_UNQUOTED([DATADIR], ["$HOME/unrealircd/data"], [Define the location of permanent data files])
		PERMDATADIR="$HOME/unrealircd/data"])

AC_ARG_WITH(docdir, [AS_HELP_STRING([--with-docdir=path],[Specify the directory where documentation is stored])],
	[AC_DEFINE_UNQUOTED([DOCDIR], ["$withval"], [Define the location of the documentation])
		DOCDIR="$withval"],
	[AC_DEFINE_UNQUOTED([DOCDIR], ["$HOME/unrealircd/doc"], [Define the location of the documentation])
		DOCDIR="$HOME/unrealircd/doc"])

AC_ARG_WITH(pidfile, [AS_HELP_STRING([--with-pidfile=path],[Specify the path of the pid file])],
	[AC_DEFINE_UNQUOTED([PIDFILE], ["$withval"], [Define the path of the pid file])
		PIDFILE="$withval"],
	[AC_DEFINE_UNQUOTED([PIDFILE], ["$HOME/unrealircd/data/unrealircd.pid"], [Define the path of the pid file])
		PIDFILE="$HOME/unrealircd/data/unrealircd.pid"])

AC_ARG_WITH(controlfile, [AS_HELP_STRING([--with-controlfile=path],[Specify the path of the control socket])],
	[AC_DEFINE_UNQUOTED([CONTROLFILE], ["$withval"], [Define the path of the control socket])
		CONTROLFILE="$withval"],
	[AC_DEFINE_UNQUOTED([CONTROLFILE], ["$HOME/unrealircd/data/unrealircd.ctl"], [Define the path of the control socket])
		CONTROLFILE="$HOME/unrealircd/data/unrealircd.ctl"])

dnl Ensure that this “feature” can be disabled as it makes it harder to package unrealircd.
dnl Users have always been able to specify “./configure LDFLAGS=-Wl,-rpath,/path/to/blah”—binki
AC_ARG_WITH(privatelibdir, [AS_HELP_STRING([--with-privatelibdir=path],[Specify the directory where private libraries are stored. Disable when building a package for a distro])],
	[],
	[with_privatelibdir="yes"])
AS_IF([test "x$with_privatelibdir" = "xno"],
	[PRIVATELIBDIR=],
	[test "x$with_privatelibdir" = "xyes"],
	[PRIVATELIBDIR="$HOME/unrealircd/lib"],
	[PRIVATELIBDIR="$with_privatelibdir"])
AS_IF([test "x$PRIVATELIBDIR" = "x"],
	[LDFLAGS_PRIVATELIBS=""],
	[AC_DEFINE_UNQUOTED([PRIVATELIBDIR], ["$PRIVATELIBDIR"], [Define the location of private libraries])
		LDFLAGS_PRIVATELIBS="-Wl,-rpath,$PRIVATELIBDIR"
		LDFLAGS="$LDFLAGS $LDFLAGS_PRIVATELIBS"
		export LDFLAGS])

AC_SUBST(BUILDDIR)
AC_SUBST(BINDIR)
AC_SUBST(SCRIPTDIR)
AC_SUBST(CONFDIR)
AC_SUBST(MODULESDIR)
AC_SUBST(LOGDIR)
AC_SUBST(CACHEDIR)
AC_SUBST(TMPDIR)
dnl Why o why PERMDATADIR and not DATADIR you ask?
dnl well, Because DATADIR conflicts with the Windows SDK header files.. amazing.
AC_SUBST(PERMDATADIR)
AC_SUBST(DOCDIR)
AC_SUBST(PIDFILE)
AC_SUBST(CONTROLFILE)
AC_SUBST(LDFLAGS_PRIVATELIBS)

AC_ARG_WITH(maxconnections, [AS_HELP_STRING([--with-maxconnections=size], [Specify the max file descriptors to use])],
	[ac_fd=$withval],
	[ac_fd=0])
AC_DEFINE_UNQUOTED([MAXCONNECTIONS_REQUEST], [$ac_fd], [Set to the maximum number of connections you want])

AC_ARG_WITH(no-operoverride, [AS_HELP_STRING([--with-no-operoverride], [Disable OperOverride])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([NO_OPEROVERRIDE], [], [Define if you want OperOverride disabled])])])
AC_ARG_WITH(operoverride-verify, [AS_HELP_STRING([--with-operoverride-verify], [Require opers to invite themselves to +s/+p channels])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([OPEROVERRIDE_VERIFY], [], [Define if you want opers to have to use /invite to join +s/+p channels])])])
AC_ARG_WITH(system-pcre2, [AS_HELP_STRING([--without-system-pcre2], [Use the system pcre2 package instead of bundled, discovered using pkg-config])], [], [with_system_pcre2=yes])
AC_ARG_WITH(system-argon2, [AS_HELP_STRING([--without-system-argon2], [Use bundled version instead of system argon2 library. Normally autodetected via pkg-config])], [], [with_system_argon2=yes])
AC_ARG_WITH(system-sodium, [AS_HELP_STRING([--without-system-sodium], [Use bundled version instead of system sodium library. Normally autodetected via pkg-config])], [], [with_system_sodium=yes])
AC_ARG_WITH(system-cares, [AS_HELP_STRING([--without-system-cares], [Use bundled version instead of system c-ares. Normally autodetected via pkg-config.])], [], [with_system_cares=yes])
AC_ARG_WITH(system-jansson, [AS_HELP_STRING([--without-system-jansson], [Use bundled version instead of system jansson. Normally autodetected via pkg-config.])], [], [with_system_jansson=yes])
CHECK_SSL
CHECK_SSL_CTX_SET1_CURVES_LIST
CHECK_SSL_CTX_SET_MIN_PROTO_VERSION
CHECK_SSL_CTX_SET_SECURITY_LEVEL
CHECK_ASN1_TIME_diff
CHECK_X509_get0_notAfter
AC_ARG_ENABLE(dynamic-linking, [AS_HELP_STRING([--disable-dynamic-linking], [Make the IRCd statically link with shared objects rather than dynamically (noone knows if disabling dynamic linking actually does anything or not)])],
	[enable_dynamic_linking=$enableval], [enable_dynamic_linking="yes"])
AS_IF([test $enable_dynamic_linking = "yes"],
	[AC_ENABLE_DYN],
	[AC_DEFINE([STATIC_LINKING], [], [Link... statically(?) (defining this macro will probably cause the build tofail)])])

AC_ARG_ENABLE([werror],
  [AS_HELP_STRING([--enable-werror],
    [Turn compilation warnings into errors (-Werror)])],
  [ac_cv_werror="$enableval"],
  [ac_cv_werror="no"])

AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--enable-asan],
    [Enable address sanitizer and other debugging options, not recommended for production servers!])],
  [ac_cv_asan="$enableval"],
  [ac_cv_asan="no"])

AC_CHECK_FUNCS([poll],
	AC_DEFINE([HAVE_POLL], [], [Define if you have poll]))
AC_CHECK_FUNCS([epoll_create epoll_ctl epoll_wait],
	AC_DEFINE([HAVE_EPOLL], [], [Define if you have epoll]))
AC_CHECK_FUNCS([kqueue kevent],
	AC_DEFINE([HAVE_KQUEUE], [], [Define if you have kqueue]))

dnl c-ares needs PATH_SEPARATOR set or it will
dnl fail on certain solaris boxes. We might as
dnl well set it here.
export PATH_SEPARATOR

dnl Use system pcre2 when available, unless --without-system-pcre2.
has_system_pcre2="no"
AS_IF([test "x$with_system_pcre2" = "xyes"],[
PKG_CHECK_MODULES([PCRE2], libpcre2-8 >= 10.00,[has_system_pcre2=yes
AS_IF([test "x$PRIVATELIBDIR" != "x"], [rm -f "$PRIVATELIBDIR/"libpcre2*])],[has_system_pcre2=no])])

AS_IF([test "$has_system_pcre2" = "no"], [
dnl REMEMBER TO CHANGE WITH A NEW PCRE2 RELEASE!
pcre2_version="10.39"
AC_MSG_RESULT(extracting PCRE2 regex library)
cur_dir=`pwd`
cd extras
dnl remove old pcre2 directory to force a recompile...
dnl and remove its installation prefix just to clean things up.
rm -rf pcre2-$pcre2_version pcre2
if test "x$ac_cv_path_GUNZIP" = "x" ; then
	tar xfz pcre2.tar.gz
else
	cp pcre2.tar.gz pcre2.tar.gz.bak
	gunzip -f pcre2.tar.gz
	cp pcre2.tar.gz.bak pcre2.tar.gz
	tar xf pcre2.tar
fi
AC_MSG_RESULT(configuring PCRE2 regex library)
cd pcre2-$pcre2_version
./configure --enable-jit --enable-shared --prefix=$cur_dir/extras/pcre2 --libdir=$PRIVATELIBDIR || exit 1
AC_MSG_RESULT(compiling PCRE2 regex library)
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing PCRE2 regex library)
$ac_cv_prog_MAKER install || exit 1
PCRE2_CFLAGS="-I$cur_dir/extras/pcre2/include"
AC_SUBST(PCRE2_CFLAGS)
PCRE2_LIBS=
dnl See c-ares's compilation section for more info on this hack.
dnl ensure that we're linking against the bundled version of pcre2
dnl (we only reach this code if linking against the bundled version is desired).
AS_IF([test -n "$ac_cv_path_PKGCONFIG"],
       [PCRE2_LIBS="`$ac_cv_path_PKGCONFIG --libs libpcre2-8.pc`"])
dnl For when pkg-config isn't available -- or for when pkg-config
dnl doesn't see the libpcre2-8.pc file somehow... (#3982)
AS_IF([test -z "$PCRE2_LIBS"],
       [PCRE2_LIBS="$PRIVATELIBDIR/libpcre2-8.so"])
AC_SUBST(PCRE2_LIBS)
cd $cur_dir
])

dnl Use system argon2 when available, unless --without-system-argon2
has_system_argon2="no"
AS_IF([test "x$with_system_argon2" = "xyes"],[
PKG_CHECK_MODULES([ARGON2], [libargon2 >= 0~20161029],[has_system_argon2=yes
AS_IF([test "x$PRIVATELIBDIR" != "x"], [rm -f "$PRIVATELIBDIR/"libargon2*])],[has_system_argon2=no])])

AS_IF([test "$has_system_argon2" = "no"],[
dnl REMEMBER TO CHANGE WITH A NEW ARGON2 RELEASE!
argon2_version="20181209"
AC_MSG_RESULT(extracting Argon2 library)
cur_dir=`pwd`
cd extras
dnl remove old argon2 directory to force a recompile...
dnl and remove its installation prefix just to clean things up.
rm -rf argon2-$argon2_version argon2
if test "x$ac_cv_path_GUNZIP" = "x" ; then
	tar xfz argon2-$argon2_version.tar.gz
else
	cp argon2-$argon2_version.tar.gz argon2-$argon2_version.tar.gz.bak
	gunzip -f argon2-$argon2_version.tar.gz
	cp argon2-$argon2_version.tar.gz.bak argon2-$argon2_version.tar.gz
	tar xf argon2-$argon2_version.tar
fi
AC_MSG_RESULT(compiling Argon2 library)
cd argon2-$argon2_version
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing Argon2 library)
$ac_cv_prog_MAKER install PREFIX=$cur_dir/extras/argon2 || exit 1
# We need to manually copy the libs to PRIVATELIBDIR because
# there is no way to tell make install in libargon2 to do so.
# BUT FIRST, delete the old library so it becomes an unlink+create
# operation rather than overwriting the existing file which would
# lead to a crash of the currently running IRCd.
rm -f "$PRIVATELIBDIR/"libargon2*
# Now copy the new library files:
cp -av $cur_dir/extras/argon2/lib/* $PRIVATELIBDIR/
ARGON2_CFLAGS="-I$cur_dir/extras/argon2/include"
AC_SUBST(ARGON2_CFLAGS)
ARGON2_LIBS="-L$PRIVATELIBDIR -largon2"
AC_SUBST(ARGON2_LIBS)
cd $cur_dir
])

dnl Use system sodium when available, unless --without-system-sodium
has_system_sodium="no"
AS_IF([test "x$with_system_sodium" = "xyes"],[
PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.16],[has_system_sodium=yes
AS_IF([test "x$PRIVATELIBDIR" != "x"], [rm -f "$PRIVATELIBDIR/"libsodium*])],[has_system_sodium=no])])

AS_IF([test "$has_system_sodium" = "no"],[
dnl REMEMBER TO CHANGE WITH A NEW SODIUM RELEASE!
sodium_version="1.0.18"
AC_MSG_RESULT(extracting sodium library)
cur_dir=`pwd`
cd extras
dnl remove old sodium directory to force a recompile...
dnl and remove its installation prefix just to clean things up.
rm -rf sodium-$sodium_version sodium
if test "x$ac_cv_path_GUNZIP" = "x" ; then
	tar xfz libsodium.tar.gz
else
	cp libsodium.tar.gz libsodium.tar.gz.bak
	gunzip -f libsodium.tar.gz
	cp libsodium.tar.gz.bak libsodium.tar.gz
	tar xf libsodium.tar
fi
AC_MSG_RESULT(compiling sodium library)
cd libsodium-$sodium_version
save_cflags="$CFLAGS"
CFLAGS="$orig_cflags"
export CFLAGS
./configure --prefix=$cur_dir/extras/sodium --libdir=$PRIVATELIBDIR --enable-shared --disable-static --enable-opt || exit 1
CFLAGS="$save_cflags"
AC_MSG_RESULT(compiling sodium resolver library)
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing sodium resolver library)
$ac_cv_prog_MAKER install || exit 1
SODIUM_CFLAGS="-I$cur_dir/extras/sodium/include"
AC_SUBST(SODIUM_CFLAGS)
SODIUM_LIBS=
dnl See c-ares's compilation section for more info on this hack.
dnl ensure that we're linking against the bundled version
dnl (we only reach this code if linking against the bundled version is desired).
AS_IF([test -n "$ac_cv_path_PKGCONFIG"],
       [SODIUM_LIBS="`$ac_cv_path_PKGCONFIG --libs libsodium.pc`"])
dnl For when pkg-config isn't available
AS_IF([test -z "$SODIUM_LIBS"],
       [SODIUM_LIBS="-L$PRIVATELIBDIR -lsodium"])
AC_SUBST(SODIUM_LIBS)
cd $cur_dir
])

dnl Use system c-ares when available, unless --without-system-cares.
has_system_cares="no"
AS_IF([test "x$with_system_cares" = "xyes"],[
PKG_CHECK_MODULES([CARES], libcares >= 1.6.0,[has_system_cares=yes
AS_IF([test "x$PRIVATELIBDIR" != "x"], [rm -f "$PRIVATELIBDIR/"libcares*])],[has_system_cares=no])])

AS_IF([test "$has_system_cares" = "no"], [
dnl REMEMBER TO CHANGE WITH A NEW C-ARES RELEASE!
dnl NOTE: when changing this here, ALSO change it in extras/curlinstall
dnl       and in the comment in this file around line 400!
cares_version="1.18.1"
AC_MSG_RESULT(extracting c-ares resolver library)
cur_dir=`pwd`
cd extras
dnl remove old c-ares directory to force a recompile...
rm -rf c-ares-$cares_version c-ares
if test "x$ac_cv_path_GUNZIP" = "x" ; then
	tar xfz c-ares.tar.gz
else
	cp c-ares.tar.gz c-ares.tar.gz.bak
	gunzip -f c-ares.tar.gz
	cp c-ares.tar.gz.bak c-ares.tar.gz
	tar xf c-ares.tar
fi
AC_MSG_RESULT(configuring c-ares library)
cd c-ares-$cares_version
save_cflags="$CFLAGS"
CFLAGS="$orig_cflags"
export CFLAGS
./configure --prefix=$cur_dir/extras/c-ares --libdir=$PRIVATELIBDIR --enable-shared --disable-tests || exit 1
CFLAGS="$save_cflags"
AC_MSG_RESULT(compiling c-ares resolver library)
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing c-ares resolver library)
$ac_cv_prog_MAKER install || exit 1
CARES_CFLAGS="-I$cur_dir/extras/c-ares/include"
AC_SUBST(CARES_CFLAGS)
CARES_LIBS="-L$PRIVATELIBDIR"

dnl Set default library parameters for when pkg-config is not available
dnl Ugly cd'ing out of extras/c-ares-xxx ;)
dnl Note: must be a full path, not relative path.
cd ../..
CARESLIBSALT="$PRIVATELIBDIR/libcares.so"
cd -
case `uname -s` in
	*FreeBSD*)
		CARESLIBSALT="$CARESLIBSALT"
		;;
	*Linux*)
		CARESLIBSALT="$CARESLIBSALT -lrt"
		;;
	*SunOS*)
		CARESLIBSALT="$CARESLIBSALT -lrt"
		;;
esac

dnl Use pkg-config for c-ares libraries, and if not available use defaults
dnl from above (also if pkg-config returns an empty result).
if test "x$ac_cv_path_PKGCONFIG" = "x" ; then
	CARES_LIBS="$CARES_LIBS $CARESLIBSALT"
else
	CARES_LIBSPRE="$CARES_LIBS"
	dnl the sed expression forces an absolute path to the .so file to be generated
	dnl because this is what libtool would do. If this wasn't done and /usr/lib*/libcares.so
	dnl exists, then unrealircd would still try to link against the system c-ares.
	dnl The [] quotation is needed because the sed expression has [] in it.
	[CARES_LIBS="$CARES_LIBS `$ac_cv_path_PKGCONFIG --libs libcares.pc | sed -e 's,-L\([^ ]\+lib\) -lcares,\1/libcares.so,'`"]
	if test "$CARES_LIBS" = "$CARES_LIBSPRE " ; then
		CARES_LIBS="$CARES_LIBS $CARESLIBSALT"
	fi
fi
AC_SUBST(CARES_LIBS)
cd $cur_dir
])

dnl Use system jansson when available, unless --without-system-jansson
has_system_jansson="no"
AS_IF([test "x$with_system_jansson" = "xyes"],[
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.0.0],[has_system_jansson=yes
AS_IF([test "x$PRIVATELIBDIR" != "x"], [rm -f "$PRIVATELIBDIR/"libjansson*])],[has_system_jansson=no])])

AS_IF([test "$has_system_jansson" = "no"],[
dnl REMEMBER TO CHANGE WITH A NEW JANSSON RELEASE!
jansson_version="2.13.1"
AC_MSG_RESULT(extracting jansson library)
cur_dir=`pwd`
cd extras
dnl remove old jansson directory to force a recompile...
dnl and remove its installation prefix just to clean things up.
rm -rf jansson-$jansson_version jansson
if test "x$ac_cv_path_GUNZIP" = "x" ; then
	tar xfz jansson.tar.gz
else
	cp jansson.tar.gz jansson.tar.gz.bak
	gunzip -f jansson.tar.gz
	cp jansson.tar.gz.bak jansson.tar.gz
	tar xf jansson.tar
fi
AC_MSG_RESULT(compiling jansson library)
cd jansson-$jansson_version
save_cflags="$CFLAGS"
CFLAGS="$orig_cflags"
export CFLAGS
./configure --prefix=$cur_dir/extras/jansson --libdir=$PRIVATELIBDIR --enable-shared --disable-static --enable-opt || exit 1
CFLAGS="$save_cflags"
AC_MSG_RESULT(compiling jansson resolver library)
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing jansson resolver library)
$ac_cv_prog_MAKER install || exit 1
JANSSON_CFLAGS="-I$cur_dir/extras/jansson/include"
AC_SUBST(JANSSON_CFLAGS)
JANSSON_LIBS=
dnl See c-ares's compilation section for more info on this hack.
dnl ensure that we're linking against the bundled version
dnl (we only reach this code if linking against the bundled version is desired).
AS_IF([test -n "$ac_cv_path_PKGCONFIG"],
       [JANSSON_LIBS="`$ac_cv_path_PKGCONFIG --libs jansson.pc`"])
dnl ^^^ FIXME FIXME this is likely incorrect the .pc etc
dnl For when pkg-config isn't available
AS_IF([test -z "$JANSSON_LIBS"],
       [JANSSON_LIBS="-L$PRIVATELIBDIR -ljansson"])
AC_SUBST(JANSSON_LIBS)
cd $cur_dir
])


AX_PTHREAD()

CHECK_LIBCURL

CHECK_GEOIP_CLASSIC

CHECK_LIBMAXMINDDB

UNRLINCDIR="`pwd`/include"

dnl Moved to the very end to ensure it doesn't affect any libs or tests.
if test "$ac_cv_werror" = "yes" ; then
	CFLAGS="$CFLAGS -Werror"
fi

dnl Address sanitizer build
if test "$ac_cv_asan" = "yes" ; then
	CFLAGS="$CFLAGS -O1 -fno-inline -fsanitize=address -fno-omit-frame-pointer -DNOCLOSEFD"
	IRCDLIBS="-fsanitize=address $IRCDLIBS"
fi

AC_SUBST(IRCDLIBS)

AC_SUBST(UNRLINCDIR)

AC_CONFIG_FILES([Makefile
	src/Makefile
	src/modules/Makefile
	src/modules/chanmodes/Makefile
	src/modules/usermodes/Makefile
	src/modules/extbans/Makefile
	src/modules/third/Makefile
	extras/unrealircd-upgrade-script
	unrealircd])
AC_OUTPUT
chmod 0700 unrealircd
