dnl aclocal.m4 generated automatically by aclocal 1.4-p4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl @synopsis ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
dnl
dnl This macro figures out how to build C programs using POSIX
dnl threads.  It sets the PTHREAD_LIBS output variable to the threads
dnl library and linker flags, and the PTHREAD_CFLAGS output variable
dnl to any special C compiler flags that are needed.  (The user can also
dnl force certain compiler flags/libs to be tested by setting these
dnl environment variables.)
dnl
dnl Also sets PTHREAD_CC to any special C compiler that is needed for
dnl multi-threaded programs (defaults to the value of CC otherwise).
dnl (This is necessary on AIX to use the special cc_r compiler alias.)
dnl
dnl If you are only building threads programs, you may wish to
dnl use these variables in your default LIBS, CFLAGS, and CC:
dnl
dnl        LIBS="$PTHREAD_LIBS $LIBS"
dnl        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
dnl        CC="$PTHREAD_CC"
dnl
dnl In addition, if the PTHREAD_CREATE_JOINABLE thread-attribute
dnl constant has a nonstandard name, defines PTHREAD_CREATE_JOINABLE
dnl to that name (e.g. PTHREAD_CREATE_UNDETACHED on AIX).
dnl
dnl ACTION-IF-FOUND is a list of shell commands to run if a threads
dnl library is found, and ACTION-IF-NOT-FOUND is a list of commands
dnl to run it if it is not found.  If ACTION-IF-FOUND is not specified,
dnl the default action will define HAVE_PTHREAD.
dnl
dnl Please let the authors know if this macro fails on any platform,
dnl or if you have any other suggestions or comments.  This macro was
dnl based on work by SGJ on autoconf scripts for FFTW (www.fftw.org)
dnl (with help from M. Frigo), as well as ac_pthread and hb_pthread
dnl macros posted by AFC to the autoconf macro repository.  We are also
dnl grateful for the helpful feedback of numerous users.
dnl
dnl @version $Id$
dnl @author Steven G. Johnson <stevenj@alum.mit.edu> and Alejandro Forero Cuervo <bachue@bachue.com>

dnl This has been recoded a lot. It now fully supports caching and is a bit faster 

AC_DEFUN([ACX_PTHREAD], [
AC_CANONICAL_HOST
acx_pthread_ok=no

# First, check if the POSIX threads header, pthread.h, is available.
# If it isn't, don't bother looking for the threads libraries.
AC_CHECK_HEADER(pthread.h, , acx_pthread_ok=noheader)

# We must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. DEC) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-POSIX).

# Create a list of thread flags to try.  Items starting with a "-" are
# C compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all.

acx_pthread_flags="pthreads none -Kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt"

# The ordering *is* (sometimes) important.  Some notes on the
# individual items follow:

# pthreads: AIX (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -Kthread and
#       other compiler flags to prevent continual compiler warnings
# -Kthread: Sequent (threads in libc, but -Kthread needed for pthread.h)
# -kthread: FreeBSD kernel threads (preferred to -pthread since SMP-able)
# lthread: LinuxThreads port on FreeBSD (also preferred to -pthread)
# -pthread: Linux/gcc (kernel threads), BSD/gcc (userland threads)
# -pthreads: Solaris/gcc
# -mthreads: Mingw32/gcc, Lynx/gcc
# -mt: Sun Workshop C (may only link SunOS threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -D_REENTRANT)
# pthread: Linux, etcetera
# --thread-safe: KAI C++

case "${host_cpu}-${host_os}" in
        *solaris*)

        # On Solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (We need to link with -pthread or
        # -lpthread.)  (The stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  So,
        # we'll just look for -pthreads and -lpthread first:

        acx_pthread_flags="-pthread -pthreads pthread -mt $acx_pthread_flags"
        ;;
esac
result=none
if test "$acx_pthread_ok" = "no"; then
AC_CACHE_CHECK(what flags pthreads needs,ac_cv_pthreadflag,[
for flag in $acx_pthread_flags; do

        case $flag in
                none)
		result="none"
                ;;

                -*)
                PTHREAD_CFLAGS="$flag"
		result="$flag"
                ;;

                *)
                PTHREAD_LIBS="-l$flag"
		result="-l$flag"
                ;;
        esac

        save_LIBS="$LIBS"
        save_CFLAGS="$CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Check for various functions.  We must include pthread.h,
        # since some functions may be macros.  (On the Sequent, we
        # need a special flag -Kthread to make this header compile.)
        # We check for pthread_join because it is in -lpthread on IRIX
        # while pthread_create is in libc.  We check for pthread_attr_init
        # due to DEC craziness with -lpthreads.  We check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on Solaris that doesn't have a non-functional libc stub.
        # We try pthread_create on general principles.
        AC_TRY_LINK([#include <pthread.h>],
                    [pthread_t th; pthread_join(th, 0);
                     pthread_attr_init(0); pthread_cleanup_push(0, 0);
                     pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
                    [acx_pthread_ok=yes])

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        if test "$acx_pthread_ok" = "yes"; then
dnl		AC_MSG_RESULT($result)
		ac_cv_pthreadflag=$result
                break;
        fi

        PTHREAD_LIBS=""
        PTHREAD_CFLAGS=""
done
fi
])

case $ac_cv_pthreadflag in
	none)
		;;
	-l*)
		PTHREAD_LIBS=$ac_cv_pthreadflag
		;;
	*)
		PTHREAD_CFLAGS=$ac_cv_pthreadflag
		;;
esac
# Various other checks:
	if test "x$ac_cv_pthreadflag"!=x; then
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Detect AIX lossage: threads are created detached by default
        # and the JOINABLE attribute has a nonstandard name (UNDETACHED).
        AC_CACHE_CHECK([for joinable pthread attribute],ac_cv_pthreadjoin,[
        AC_TRY_LINK([#include <pthread.h>],
                    [int attr=PTHREAD_CREATE_JOINABLE;],
                    ac_cv_pthreadjoin=PTHREAD_CREATE_JOINABLE, ac_cv_pthreadjoin=unknown)
        if test "$ac_cv_pthreadjoin" = "unknown"; then
                AC_TRY_LINK([#include <pthread.h>],
                            [int attr=PTHREAD_CREATE_UNDETACHED;],
                            ac_cv_pthreadjoin=PTHREAD_CREATE_UNDETACHED, ac_cv_pthreadjoin=unknown)
        fi
	])
        if test "$ac_cv_pthreadjoin" != "PTHREAD_CREATE_JOINABLE"; then
                AC_DEFINE(PTHREAD_CREATE_JOINABLE, $ac_cv_pthreadjoin,
                          [Define to the necessary symbol if this constant
                           uses a non-standard name on your system.])
        fi
        if test "$ac_cv_pthreadjoin" = "unknown"; then
                AC_MSG_WARN([we do not know how to create joinable pthreads])
        fi

        AC_CACHE_CHECK([if more special flags are required for pthreads],ac_cv_pthreadspecial,[
        ac_cv_pthreadspecial=no
        case "${host_cpu}-${host_os}" in
                *-aix* | *-freebsd*)     ac_cv_pthreadspecial="-D_THREAD_SAFE";;
                *solaris* | alpha*-osf*) ac_cv_pthreadspecial="-D_REENTRANT";;
        esac
	])
        if test "$ac_cv_pthreadspecial" != "no"; then
                PTHREAD_CFLAGS="$ac_cv_pthreadspecial $PTHREAD_CFLAGS"
        fi

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        # More AIX lossage: must compile with cc_r
        AC_CHECK_PROG(PTHREAD_CC, cc_r, cc_r, ${CC})
else
        PTHREAD_CC="$CC"
fi
if test "x$ac_cv_pthreadflag"!=x; then
AC_CACHE_CHECK(if pthreads uses one thread per process, ac_cv_thread_multi, [
save_LIBS="$LIBS"
LIBS="$PTHREAD_LIBS $LIBS"
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
AC_TRY_RUN([
#include <pthread.h>
int pid;
int mypid = -1;
pthread_mutex_t mutex;
void testthreads(void *p)
{
	pthread_mutex_lock(&mutex);
	mypid = getpid();
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}
int main() {
	int	i;
	pthread_t thread;
	pthread_attr_t attrs;

	pid = getpid();
	pthread_attr_init(&attrs);
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_lock(&mutex);
	pthread_create(&thread, &attrs, (void*)testthreads, NULL);
	pthread_mutex_unlock(&mutex);
	sleep(2);
	pthread_mutex_lock(&mutex);
	if (mypid == pid)
		exit(0);
	else
		exit(1);
}
],ac_cv_thread_multi=no, ac_cv_thread_multi=yes)
LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"
])
if test "$USESTDTHREAD" != "1"; then
if test "$ac_cv_thread_multi" = "yes"; then
AC_MSG_RESULT(Ok we'll install FSU Pthreads)
cd extras
if [ -f "pthreads.tar.gz" ] ; then 
	gunzip -d pthreads.tar.gz
fi
tar xf pthreads.tar
cd threads/src
./configure
cd ../../../
if test "$ac_cv_pthreadspecial" != no; then
PTHREAD_CFLAGS="-I=../extras/threads/include $ac_cv_pthreadspecial"
else
PTHREAD_CFLAGS="-I=../extras/threads/include"
fi
PTHREAD_LIBS="../extras/threads/lib/libgthreads.a ../extras/threads/lib/libmalloc.a"
fi
fi
fi
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_CC)

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x"$acx_pthread_ok" = xyes; then
        ifelse([$1],,AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.]),[$1])
        :
else
        acx_pthread_ok=no
        $2
fi

])dnl ACX_PTHREAD

