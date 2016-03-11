dnl Process this file with autoconf to produce a configure script.

dnl When updating the version, remember to update the following files
dnl appropriately:
dnl
dnl include/win32/setup.h
dnl src/win32/unrealinst.iss
dnl .CHANGES.NEW
dnl src/version.c.SH

AC_INIT([unrealircd], [4.0.2], [http://bugs.unrealircd.org/], [], [http://unrealircd.org/])
AC_CONFIG_SRCDIR([src/ircd.c])
AC_CONFIG_HEADER([include/setup.h])
AC_CONFIG_AUX_DIR([autoconf])
AC_CONFIG_MACRO_DIR([autoconf/m4])

if test "x$enable_dynamic_linking" = "x"; then
	echo "Please use ./Config instead of ./configure"
	exit 1
fi

dnl Calculate the versions. Perhaps the use of expr is a little too extravagant
# Generation version number (e.g.: X in X.Y.Z)
UNREAL_VERSION_GENERATION=["4"]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_GENERATION], [$UNREAL_VERSION_GENERATION], [Generation version number (e.g.: X for X.Y.Z)])

# Major version number (e.g.: Y in X.Y.Z)
UNREAL_VERSION_MAJOR=["0"]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_MAJOR], [$UNREAL_VERSION_MAJOR], [Major version number (e.g.: Y for X.Y.Z)])

# Minor version number (e.g.: Z in X.Y.Z)
UNREAL_VERSION_MINOR=["2"]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_MINOR], [$UNREAL_VERSION_MINOR], [Minor version number (e.g.: Z for X.Y.Z)])

# The version suffix such as a beta marker or release candidate
# marker. (e.g.: -rcX for unrealircd-3.2.9-rcX). This macro is a
# string instead of an integer because it contains arbitrary data.
UNREAL_VERSION_SUFFIX=[""]
AC_DEFINE_UNQUOTED([UNREAL_VERSION_SUFFIX], ["$UNREAL_VERSION_SUFFIX"], [Version suffix such as a beta marker or release candidate marker. (e.g.: -rcX for unrealircd-3.2.9-rcX)])

AC_PROG_CC
if test "$ac_cv_prog_gcc" = "yes"; then
CFLAGS="$CFLAGS -funsigned-char"
AC_CACHE_CHECK(if gcc has a working -pipe, ac_cv_pipe, [
	save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS -pipe"
	AC_TRY_COMPILE(,, ac_cv_pipe="yes", ac_cv_pipe="no")
	CFLAGS="$save_cflags"
])
if test "$ac_cv_pipe" = "yes"; then
CFLAGS="-pipe $CFLAGS"
fi
fi

dnl (the gcc flag detection trick is taken from xemacs/Vin Shelton)

dnl UnrealIRCd might not be strict-aliasing safe at this time
case "`$CC -v --help 2>&1`" in
	*-fstrict-aliasing* ) CFLAGS="$CFLAGS -fno-strict-aliasing" ;;
esac

dnl Pointer signedness warnings are really a pain and 99.9% of the time
dnl they are of absolutely no use whatsoever. IMO the person who decided
dnl to enable this without -Wall should be shot on sight.
case "`$CC -v --help 2>&1`" in
	*-Wpointer-sign* ) CFLAGS="$CFLAGS -Wno-pointer-sign" ;;
esac

AC_PATH_PROG(RM,rm)
AC_PATH_PROG(CP,cp)
AC_PATH_PROG(TOUCH,touch)
AC_PATH_PROG(OPENSSLPATH,openssl)
AC_PATH_PROG(INSTALL,install)
AC_CHECK_PROG(MAKER, gmake, gmake, make)
AC_PATH_PROG(GMAKE,gmake)
AC_PATH_PROG(GUNZIP, gunzip)
AC_PATH_PROG(PKGCONFIG, pkg-config)

dnl Checks for libraries.
AC_CHECK_LIB(descrypt, crypt,
	[AC_DEFINE([HAVE_CRYPT], [], [Define if you have crypt])
		IRCDLIBS="$IRCDLIBS-ldescrypt "
		MKPASSWDLIBS="-ldescrypt"],
	[AC_CHECK_LIB(crypt, crypt,
		[AC_DEFINE([HAVE_CRYPT], [], [Define if you have crypt])
			IRCDLIBS="$IRCDLIBS-lcrypt "
			MKPASSWDLIBS="-lcrypt"])])
AC_CHECK_LIB(socket, socket,
	[IRCDLIBS="$IRCDLIBS-lsocket "
		SOCKLIB="-lsocket"])
AC_CHECK_LIB(nsl, inet_ntoa,
	[IRCDLIBS="$IRCDLIBS-lnsl "
		INETLIB="-lnsl"])
AC_CHECK_LIB(crypto, RAND_egd,
		AC_DEFINE(HAVE_RAND_EGD, 1, [Define if the libcrypto has RAND_egd]))

AC_SUBST(IRCDLIBS)
AC_SUBST(MKPASSWDLIBS)

dnl module checking based on Hyb7's module checking code
AC_DEFUN([AC_ENABLE_DYN],
[
AC_CHECK_FUNC(dlopen,, [AC_CHECK_LIB(dl, dlopen,IRCDLIBS="$IRCDLIBS -ldl",
[
AC_MSG_WARN(Dynamic linking is not enabled because dlopen was not found) 
AC_DEFINE(STATIC_LINKING)
])])

hold_cflags=$CFLAGS
CFLAGS="$CFLAGS -Wl,-export-dynamic"
AC_CACHE_CHECK(if we need the -export-dynamic flag, ac_cv_export_dynamic, [
AC_TRY_LINK(, [int i];, ac_cv_export_dynamic=yes, ac_cv_export_dynamic=no)])
if test "$ac_cv_export_dynamic" = "no"; then
CFLAGS=$hold_cflags
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

MODULEFLAGS=$ac_cv_pic
dnl DYNAMIC_LINKING is not meant to be defined in include/setup.h, it's 
dnl defined in the Makefiles using -D. Having it defined globally will
dnl only cause braindamage and symbol collisions :-D.
dnl AC_DEFINE([DYNAMIC_LINKING], [], [Link dynamically as opposed to statically. (Dynamic linking is the only supported method of linking atm)])
])

AC_CACHE_CHECK([if your system has IPv6 support], [ac_cv_ip6], [
save_libs="$LIBS"
LIBS="$LIBS $SOCKLIB"
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
LIBS="$save_libs"

AC_CHECK_HEADER(sys/param.h,
	AC_DEFINE([PARAMH], [], [Define if you have the <sys/param.h> header file.]))
AC_CHECK_HEADER(stdlib.h,
	AC_DEFINE([STDLIBH], [], [Define if you have the <stdlib.h> header file.]))
AC_CHECK_HEADER(stddef.h,
	AC_DEFINE([STDDEFH], [], [Define if you have the <stddef.h> header file.]))
AC_CHECK_HEADER(sys/syslog.h,
	AC_DEFINE([SYSSYSLOGH], [], [Define if you have the <sys/syslog.h> header file.]))
AC_CHECK_HEADER(unistd.h,
	AC_DEFINE([UNISTDH], [], [Define if you have the <unistd.h> header file.]))
AC_CHECK_HEADER(string.h,
	AC_DEFINE([STRINGH], [], [Define if you have the <string.h> header file.]))
AC_CHECK_HEADER(strings.h,
	AC_DEFINE([STRINGSH], [], [Define if you have the <strings.h> header file.]))
AC_CHECK_HEADER(malloc.h,
	AC_DEFINE([MALLOCH], [<malloc.h>], [Define to <malloc.h> you need malloc.h.]))
AC_CHECK_HEADER(sys/rusage.h,
	AC_DEFINE([RUSAGEH], [], [Define if you have the <sys/rusage.h> header file.]))
AC_CHECK_HEADER(glob.h,
	AC_DEFINE([GLOBH], [], [Define if you have the <glob.h> header file.]))
AC_CHECK_HEADERS([stdint.h inttypes.h])
dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_INTPTR_T

AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T
unreal_CHECK_TYPE_SIZES

dnl in the future, it would be nice to avoid AC_TRY_RUN to allow
dnl better support for crosscompiling.
AC_CACHE_CHECK([what kind of nonblocking sockets you have], [ac_cv_nonblocking],[
save_libs="$LIBS"
LIBS="$LIBS $SOCKLIB"
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
alarmed() {
exit(1);
}
int main() {
#ifdef O_NONBLOCK
char b[12], x[32];
int f, l = sizeof(x);
f = socket(AF_INET, SOCK_DGRAM, 0);
if (f >= 0 && !(fcntl(f, F_SETFL, O_NONBLOCK))) {
signal(SIGALRM, alarmed);
alarm(3);
recvfrom(f, b, 12, 0, (struct sockaddr *)x, &l);
alarm(0);
exit(0);
}
#endif
exit(1);
}
],ac_cv_nonblocking=O_NONBLOCK,[
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
alarmed() {
exit(0);
}
int main() {
#ifdef O_NDELAY
char b[12], x[32];
int f, l = sizeof(x);
f = socket(AF_INET, SOCK_DGRAM, 0);
if (f >= 0 && !(fcntl(f, F_SETFL, O_NDELAY))) {
signal(SIGALRM, alarmed);
alarm(3);
recvfrom(f, b, 12, 0, (struct sockaddr *)x, &l);
alarm(0);
exit(0);
}
#endif
exit(1);
}],ac_cv_nonblocking=O_NDELAY,[
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
alarmed() {
exit(1);
}
int main() {
#ifdef FIONBIO
char b[12], x[32];
int f, l = sizeof(x);
f = socket(AF_INET, SOCK_DGRAM, 0);
if (f >= 0 && !(fcntl(f, F_SETFL, FIONBIO))) {
signal(SIGALRM, alarmed);
alarm(3);
recvfrom(f, b, 12, 0, (struct sockaddr *)x, &l);
alarm(0);
exit(0);
}
#endif
exit(1);
], ac_cv_nonblocking=FIONBIO,ac_cv_nonblocking=none)])])])
if test "$ac_cv_nonblocking" = "O_NONBLOCK"; then
	AC_DEFINE([NBLOCK_POSIX], [], [Define if you have O_NONBLOCK])
elif test "$ac_cv_nonblocking" = "O_NDELAY"; then
	AC_DEFINE([NBLOCK_BSD], [], [Define if you have O_NDELAY])
elif test "$ac_cv_nonblocking" = "FIONBIO"; then
	AC_DEFINE([NBLOCK_SYSV], [], [Define if you have FIONBIO])
fi
LIBS="$save_libs"
dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_SETPGRP
AC_FUNC_SETVBUF_REVERSED
AC_CHECK_FUNCS(snprintf,
	AC_DEFINE([HAVE_SNPRINTF], [], [Define if you have snprintf]))
AC_CHECK_FUNCS(vsnprintf,
	AC_DEFINE([HAVE_VSNPRINTF], [], [Define if you have vsnprintf]))
AC_CHECK_FUNCS(strlcpy,
	AC_DEFINE([HAVE_STRLCPY], [], [Define if you have strlcpy. Otherwise, an internal implementation will be used!]))
AC_CHECK_FUNCS(strlcat,
	AC_DEFINE([HAVE_STRLCAT], [], [Define if you have strlcat]))
AC_CHECK_FUNCS(strlncat,
	AC_DEFINE([HAVE_STRLNCAT], [], [Define if you have strlncat]))
AC_CHECK_FUNCS(inet_pton,
	AC_DEFINE([HAVE_INET_PTON], [], [Define if you have inet_pton]))
AC_CHECK_FUNCS(inet_ntop,
	AC_DEFINE([HAVE_INET_NTOP], [], [Define if you have inet_ntop]))
dnl Check if it supports C99 style variable length arrays
AC_CACHE_CHECK([if C99 variable length arrays are supported], [ac_cv_varlen_arrays], [
AC_TRY_COMPILE(,[
int main() {
int i = 5;
int a[i];
a[0] = 1;
return 0;
}], ac_cv_varlen_arrays=yes, ac_cv_varlen_arrays=no)
])
if test "$ac_cv_varlen_arrays" = "yes" ; then
	AC_DEFINE([HAVE_C99_VARLEN_ARRAY], [], [Define if you have a compiler with C99 variable length array support])
fi

dnl This check doesn't need to be in ./configure, we can
dnl write the sourcecode to actually handle the return value
dnl of setrlimit if necessary... -- ohnobinki
AC_CACHE_CHECK([if we can set the core size to unlimited], [ac_cv_force_core], [
AC_TRY_RUN([
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
int main() {
struct rlimit corelim;
corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
if (setrlimit(RLIMIT_CORE, &corelim))
exit(1);
exit(0);
}
],ac_cv_force_core=yes,ac_cv_force_core=no)
])
if test "$ac_cv_force_core" = "yes"; then
	AC_DEFINE([FORCE_CORE], [], [Define if you can set the core size to unlimited])
fi
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday],
	[AC_DEFINE([GETTIMEOFDAY], [], [Define if you have gettimeofday])],
	[AC_CHECK_FUNCS([lrand48],
		[AC_DEFINE([LRADN48], [], [Define if you have lrand48])])])
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


AC_CACHE_CHECK([what type of signals you have], [ac_cv_sigtype], [
AC_TRY_RUN([
#include <signal.h>
int main() {
sigaction(SIGTERM, (struct sigaction *)0L, (struct sigaction *)0L);
}
], ac_cv_sigtype=POSIX, [
AC_TRY_RUN([
#include <signal.h>
int	calls = 0;
void	handler()
{
if (calls)
return;
calls++;
kill(getpid(), SIGTERM);
sleep(1);
}

int main() {
signal(SIGTERM, handler);
kill(getpid(), SIGTERM);
exit(0);
}
], ac_cv_sigtype=BSD,ac_cv_sigtype=SYSV)])])

if test "$ac_cv_sigtype" = "POSIX"; then
	AC_DEFINE([POSIX_SIGNALS], [], [Define if you have POSIX signals])
elif test "$ac_cv_sigtype" = "BSD"; then
	AC_DEFINE([BSD_RELIABLE_SIGNALS], [], [Define if you have BSD signals])
else
	AC_DEFINE([SYSV_UNRELIABLE_SIGNALS], [], [Define if you have SYSV signals])
fi 
AC_CHECK_FUNCS(strtoken,,AC_DEFINE([NEED_STRTOKEN], [], [Define if you need the strtoken function.]))
AC_CHECK_FUNCS(strtok,,AC_DEFINE([NEED_STRTOK], [], [Define if you need the strtok function.]))
AC_CHECK_FUNCS(strerror,,AC_DEFINE([NEED_STRERROR], [], [Define if you need the strerror function.]))
AC_CHECK_FUNCS(index,,AC_DEFINE([NOINDEX], [], [Define if you do not have the index function.]))
AC_CHECK_FUNCS(strtoul,,STRTOUL="strtoul.o")
AC_CHECK_FUNCS(bcopy,,AC_DEFINE([NEED_BCOPY], [], [Define if you don't have bcopy]))
AC_CHECK_FUNCS(bcmp,,AC_DEFINE([NEED_BCMP], [], [Define if you don't have bcmp]))
AC_CHECK_FUNCS(bzero,,AC_DEFINE([NEED_BZERO], [], [Define if you need bzero]))
AC_CHECK_FUNCS(strcasecmp,AC_DEFINE([GOT_STRCASECMP], [], [Define if you have strcasecmp]))
save_libs="$LIBS"
LIBS="$LIBS $SOCKLIB $INETLIB"
AC_CHECK_FUNCS(inet_addr,,AC_DEFINE([NEED_INET_ADDR], [], [Define if you need inet_addr]))
AC_CHECK_FUNCS(inet_ntoa,,AC_DEFINE([NEED_INET_NTOA], [], [Define if you need inet_ntoa]))
LIBS="$save_libs"
AC_CHECK_FUNCS(syslog,AC_DEFINE([HAVE_SYSLOG], [], [Define if you have syslog]))
AC_SUBST(STRTOUL)
AC_SUBST(CRYPTOLIB)
AC_SUBST(MODULEFLAGS)
AC_ARG_WITH(nick-history, [AS_HELP_STRING([--with-nick-history=length],[Specify the length of the nickname history])],
	[AC_DEFINE_UNQUOTED([NICKNAMEHISTORYLENGTH], [$withval], [Set to the nickname history length you want])],
	[AC_DEFINE([NICKNAMEHISTORYLENGTH], [2000], [Set to the nickname history length you want])])
AC_ARG_WITH([sendq], [AS_HELP_STRING([--with-sendq=maxsendq],[Specify the max sendq for the server])],
	[AC_DEFINE_UNQUOTED([MAXSENDQLENGTH], [$withval], [Set to the max sendq you want])],
	[AC_DEFINE([MAXSENDQLENGTH], [3000000], [Set to the max sendq you want])])
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

AC_ARG_WITH(fd-setsize, [AS_HELP_STRING([--with-fd-setsize=size], [Specify the max file descriptors to use])],
	[ac_fd=$withval],
	[ac_fd=1024])
AC_DEFINE_UNQUOTED([MAXCONNECTIONS], [$ac_fd], [Set to the max connections you want])

AC_ARG_ENABLE([prefixaq],
	[AS_HELP_STRING([--disable-prefixaq],[Disable chanadmin (+a) and chanowner (+q) prefixes])],
	[],
	[enable_prefixaq=yes])
AS_IF([test $enable_prefixaq = "yes"],
	[AC_DEFINE([PREFIX_AQ], [], [Define if you want +a/+q prefixes])])

AC_ARG_WITH(showlistmodes,
	[AS_HELP_STRING([--with-showlistmodes], [Specify whether modes are shown in /list])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([LIST_SHOW_MODES], [], [Define if you want modes shown in /list])])])
AC_ARG_WITH(topicisnuhost, [AS_HELP_STRING([--with-topicisnuhost], [Display nick!user@host as the topic setter])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([TOPIC_NICK_IS_NUHOST], [], [Define if you want nick!user@host shown for the topic setter])])])
AC_ARG_WITH(shunnotices, [AS_HELP_STRING([--with-shunnotices], [Notify a user when he/she is no longer shunned])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([SHUN_NOTICES], [], [Define if you want users to be notified when their shun is removed])])])
AC_ARG_WITH(no-operoverride, [AS_HELP_STRING([--with-no-operoverride], [Disable OperOverride])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([NO_OPEROVERRIDE], [], [Define if you want OperOverride disabled])])])
AC_ARG_WITH(disableusermod, [AS_HELP_STRING([--with-disableusermod], [Disable /set* and /chg*])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([DISABLE_USERMOD], [], [Define if you want to disable /set* and /chg*])])])
AC_ARG_WITH(operoverride-verify, [AS_HELP_STRING([--with-operoverride-verify], [Require opers to invite themselves to +s/+p channels])],
	[AS_IF([test $withval = "yes"],
		[AC_DEFINE([OPEROVERRIDE_VERIFY], [], [Define if you want opers to have to use /invite to join +s/+p channels])])])
AC_ARG_WITH(disable-extendedban-stacking, [AS_HELP_STRING([--with-disable-extendedban-stacking], [Disable extended ban stacking])],
       [AS_IF([test $withval = "yes"],
       		[AC_DEFINE([DISABLE_STACKED_EXTBANS], [], [Define to disable extended ban stacking (~q:~c:\#chan, etc)])])])
AC_ARG_WITH(system-tre, [AS_HELP_STRING([--with-system-tre], [Use the system tre package instead of bundled, discovered using pkg-config])], [], [with_system_tre=no])
AC_ARG_WITH(system-pcre2, [AS_HELP_STRING([--with-system-pcre2], [Use the system pcre2 package instead of bundled, discovered using pkg-config])], [], [with_system_pcre2=no])
AC_ARG_WITH(system-cares, [AS_HELP_STRING([--with-system-cares], [Use the system c-ares (at least version 1.6.0) package instead of bundled c-ares, discovered using pkg-config])], [], [with_system_cares=no])
CHECK_SSL
AC_ARG_ENABLE(dynamic-linking, [AS_HELP_STRING([--disable-dynamic-linking], [Make the IRCd statically link with shared objects rather than dynamically (noone knows if disabling dynamic linking actually does anything or not)])],
	[enable_dynamic_linking=$enableval], [enable_dynamic_linking="yes"])
AS_IF([test $enable_dynamic_linking = "yes"],
	[AC_ENABLE_DYN],
	[AC_DEFINE([STATIC_LINKING], [], [Link... statically(?) (defining this macro will probably cause the build tofail)])])

AC_MSG_CHECKING([if FD_SETSIZE is large enough to allow $ac_fd file descriptors])
AC_COMPILE_IFELSE([
#include <sys/types.h>
#include <sys/time.h>
int main() {
#if FD_SETSIZE < $ac_fd
#error FD_SETSIZE is smaller than $ac_fd
#endif
exit(0);
}
], AC_MSG_RESULT([yes]), [
# must be passed on the commandline to avoid a ``warning, you redefined something''
FD_SETSIZE="-DFD_SETSIZE=$ac_fd"
AC_MSG_RESULT(no)
])
AC_SUBST([FD_SETSIZE])

case `uname -s` in
	*SunOS*|*solaris*)
		AC_DEFINE([_SOLARIS], [], [Define if you are compiling unrealircd on Sun's (or Oracle's?) Solaris])
		IRCDLIBS="$IRCDLIBS -lresolv "
		;;
esac

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

AS_IF([test "x$with_system_tre" = "xno"],[
dnl REMEMBER TO CHANGE WITH A NEW TRE RELEASE!
tre_version="0.8.0"
AC_MSG_RESULT(extracting TRE regex library)
cur_dir=`pwd`
cd extras
dnl remove old tre directory to force a recompile...
dnl and remove its installation prefix just to clean things up.
rm -rf tre-$tre_version rege[]xp
if test "x$ac_cv_path_GUNZIP" = "x" ; then
	tar xfz tre.tar.gz
else
	cp tre.tar.gz tre.tar.gz.bak
	gunzip -f tre.tar.gz
	cp tre.tar.gz.bak tre.tar.gz
	tar xf tre.tar
fi
AC_MSG_RESULT(configuring TRE regex library)
cd tre-$tre_version
./configure --disable-agrep --disable-shared --disable-system-abi --disable-wchar --disable-multibyte --prefix=$cur_dir/extras/regexp || exit 1
AC_MSG_RESULT(compiling TRE regex library)
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing TRE regex library)
$ac_cv_prog_MAKER install || exit 1
TRE_CFLAGS="-I$cur_dir/extras/regexp/include"
AC_SUBST(TRE_CFLAGS)

TRE_LIBS=
dnl See c-ares's compilation section for more info on this hack.
dnl ensure that we're linking against the bundled version of tre
dnl (we only reach this code if linking against the bundled version is desired).
AS_IF([test -n "$ac_cv_path_PKGCONFIG"],
       [TRE_LIBS="`$ac_cv_path_PKGCONFIG --static --libs tre.pc | sed -e ['s,-L\([^ ]*regex[^ ]*lib\)\( .*\)-ltre,\1/libtre.a \2,']`"])
dnl For when pkg-config isn't available -- or for when pkg-config
dnl doesn't see the tre.pc file somehow... (#3982)
AS_IF([test -z "$TRE_LIBS"],
       [TRE_LIBS="../extras/regexp/lib/libtre.a"])
AC_SUBST(TRE_LIBS)
cd $cur_dir
],[
dnl use pkgconfig for tre:
PKG_CHECK_MODULES([TRE], tre >= 0.7.5)
])

AS_IF([test "x$with_system_pcre2" = "xno"],[
dnl REMEMBER TO CHANGE WITH A NEW PCRE2 RELEASE!
pcre2_version="10.20"
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
./configure --enable-jit --disable-shared --disable-unicode --prefix=$cur_dir/extras/pcre2 || exit 1
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
       [PCRE2_LIBS="`$ac_cv_path_PKGCONFIG --static --libs libpcre2-8.pc | sed -e ['s,-L\([^ ]*regex[^ ]*lib\)\( .*\)-lpcre2-8,\1/libpcre2-8.a \2,']`"])
dnl For when pkg-config isn't available -- or for when pkg-config
dnl doesn't see the libpcre2-8.pc file somehow... (#3982)
AS_IF([test -z "$PCRE2_LIBS"],
       [PCRE2_LIBS="../extras/pcre2/lib/libpcre2-8.a"])
AC_SUBST(PCRE2_LIBS)
cd $cur_dir
],[
dnl use pkgconfig for pcre2:
PKG_CHECK_MODULES([PCRE2], libpcre2-8 >= 10.00)
])

AS_IF([test "x$with_system_cares" = "xno"],[
dnl REMEMBER TO CHANGE WITH A NEW C-ARES RELEASE!
dnl NOTE: when changing this here, ALSO change it in extras/curlinstall
dnl       and in the comment in this file around line 400!
cares_version="1.10.0"
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
./configure --prefix=$cur_dir/extras/c-ares --disable-shared || exit 1
AC_MSG_RESULT(compiling c-ares resolver library)
$ac_cv_prog_MAKER || exit 1
AC_MSG_RESULT(installing c-ares resolver library)
$ac_cv_prog_MAKER install || exit 1
CARES_CFLAGS="-I$cur_dir/extras/c-ares/include"
AC_SUBST(CARES_CFLAGS)
CARES_LIBS="-L../extras/c-ares/lib"

dnl Set default library parameters for when pkg-config is not available
dnl Ugly cd'ing out of extras/c-ares-xxx ;)
dnl Note: must be a full path, not relative path.
cd ../..
CARESLIBSALT="`pwd`/extras/c-ares/lib/libcares.a"
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
dnl from above. Also, if pkg-config returns an empty result (ex: on fbsd
dnl or older versions it might be missing --static) then also use defaults.
if test "x$ac_cv_path_PKGCONFIG" = "x" ; then
	CARES_LIBS="$CARES_LIBS $CARESLIBSALT"
else
	CARES_LIBSPRE="$CARES_LIBS"
	dnl the sed expression forces an absolute path to the .a file to be generated
	dnl because this is what libtool would do. If this wasn't done and /usr/lib*/libcares.so
	dnl exists, then unrealircd would still try to link against the system c-ares.
	dnl The [] quotation is needed because the sed expression has [] in it.
	[CARES_LIBS="$CARES_LIBS `$ac_cv_path_PKGCONFIG --static --libs libcares.pc | sed -e 's,-L\([^ ]\+lib\) -lcares,\1/libcares.a,'`"]
	if test "$CARES_LIBS" = "$CARES_LIBSPRE " ; then
		CARES_LIBS="$CARES_LIBS $CARESLIBSALT"
	fi
fi
AC_SUBST(CARES_LIBS)
cd $cur_dir
],[
dnl use pkg-config for system c-ares:
PKG_CHECK_MODULES([CARES], libcares >= 1.6.0)
])

AX_PTHREAD()

CHECK_LIBCURL

UNRLINCDIR="`pwd`/include"
AC_SUBST(UNRLINCDIR)

BUILDDIR="`pwd`"
AC_SUBST(BUILDDIR)

AC_CONFIG_FILES([Makefile
	src/modules/Makefile
	src/modules/chanmodes/Makefile
	src/modules/usermodes/Makefile
	src/modules/snomasks/Makefile
	src/modules/extbans/Makefile
	src/modules/third/Makefile
	unrealircd])
AC_OUTPUT
chmod 0700 unrealircd
