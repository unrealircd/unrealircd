dnl Process this file with autoconf to produce a configure script.
AC_INIT(TRE, 0.5.3, [Ville Laurikari <vl@iki.fi>])
AC_CONFIG_SRCDIR([lib/regcomp.c])
AC_CONFIG_AUX_DIR(utils)
AC_CANONICAL_TARGET([])
AM_INIT_AUTOMAKE
AC_PREREQ(2.57)

dnl Checks for programs.
CFLAGS="-g"
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

dnl Options
AC_ARG_ENABLE(profile,
  AC_HELP_STRING([--enable-profile],
                 [enable profiling with gprof @<:@default=disabled@:>@]),
  [ tre_profile="$enableval" ],
  [ tre_profile="no" ])
if test "$tre_profile" = "yes"; then
  CFLAGS="$CFLAGS -pg"
  AM_CONDITIONAL(TRE_PROFILE, true)
else
  AM_CONDITIONAL(TRE_PROFILE, false)
fi

AC_ARG_ENABLE(debug,
  AC_HELP_STRING(
    [--enable-debug],
    [enable development-time debugging @<:@default=disabled@:>@]),
  [ tre_debug="$enableval" ],
  [ tre_debug="no" ])
if test "$tre_debug" = "yes"; then
  AM_CONDITIONAL(TRE_DEBUG, true)
  AC_DEFINE(TRE_DEBUG, 1,
    [Define if you want TRE to print debug messages to stdout.])
else
  AM_CONDITIONAL(TRE_DEBUG, false)
  TRE_PROG_CC_OPTIMIZATIONS
  AC_DEFINE(NDEBUG, 1, [Define if you want to disable debug assertions.])
fi

AC_ARG_ENABLE(warnings,
  AC_HELP_STRING(
    [--disable-warnings],
    [disable C compiler warning messages @<:@default=enabled@:>@]),
  [ tre_cc_warnings="$enableval" ],
  [ tre_cc_warnings="yes" ])
if test "$tre_cc_warnings" = "yes"; then
  VL_PROG_CC_WARNINGS()
fi

AC_ARG_ENABLE(approx,
  AC_HELP_STRING(
    [--disable-approx],
    [disable the approximate matching functionality @<:@default=enabled@:>@]),
  [ tre_approx="$enableval" ],
  [ tre_approx="yes" ])
if test "$tre_approx" = "yes"; then
  AC_DEFINE(TRE_APPROX, 1,
    [Define if you want to enable approximate matching functionality.])
  AM_CONDITIONAL(TRE_APPROX, true)
else
  AM_CONDITIONAL(TRE_APPROX, false)
fi

AC_ARG_ENABLE(agrep,
  AC_HELP_STRING([--disable-agrep],
    [Do not build and install the agrep tool @<:@default=install@:>@]),
  [ tre_agrep="$enableval" ],
  [ tre_agrep="yes" ])

if test "$tre_agrep" = "yes"; then
  AM_CONDITIONAL(TRE_AGREP, true)
else
  AM_CONDITIONAL(TRE_AGREP, false)
fi

dnl Checks for compiler characteristics.
AC_C_CONST
AC_C_INLINE

dnl Checks for headers, functions, types, and macros
AC_HEADER_STDC
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([getopt.h])

AC_CHECK_FUNCS([getopt_long],,
  [ # FreeBSD has a "gnugetopt" library.
    AC_CHECK_LIB([gnugetopt], [getopt_long],
                 [ AC_DEFINE([HAVE_GETOPT_LONG]) ]) ])

AC_ARG_ENABLE(system-abi,
  AC_HELP_STRING(
    [--disable-system-abi],
    [don't include system regex.h from TRE regex.h  @<:@default=enabled@:>@]),
  [ tre_system_abi="$enableval" ],
  [ tre_system_abi="yes" ])

# If we are building a version compatible with the system ABI, we need to
# find an absolute path to the system regex.h (so it can be included from
# TRE regex.h using `#include "/path/to/regex.h"').  Then we need to
# find a field in the system defined regex_t struct where a pointer to
# a TNFA can be stored.

if test "$tre_system_abi" = "yes"; then
  # Check that there is a system regex.h to beging with.
  AC_CHECK_HEADERS([sys/types.h])
  AC_CHECK_HEADERS([regex.h], [],
    [ tre_system_abi="no (regex.h not found)" ],
    [#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
])
else
  tre_system_abi="no (disabled with --disable-system-abi)"
fi

if test "$tre_system_abi" = "yes"; then
  # Find out where system <regex.h> is.
  AC_MSG_CHECKING([path to system regex.h])
  echo '#include <regex.h>' > conftest.c
  tre_system_regex_h=`$CPP conftest.c \
    | grep '^#[a-z]* [0-9]* "\(.*regex.h\)"' \
    | head -1 \
    | sed 's/^#[a-z]* [0-9]* \"\(.*\)\".*/\1/'`
  rm -f conftest.c
  if test -n "$tre_system_regex_h" && test -f "$tre_system_regex_h"; then
    AC_MSG_RESULT($tre_system_regex_h)
  else
    AC_MSG_RESULT(unknown)
    tre_system_abi="no (could determine path to systeg regex.h)"
  fi
fi

if test "$tre_system_abi" = "yes"; then
  # Find a field in the system regex_t struct where we can store a pointer.
  AC_MSG_CHECKING([for a usable field for a pointer in regex_t])
  tre_regex_t_field=""
  for field in buffer re_comp __re_comp __reg_expression \
               re_g "re_pad@<:@0@:>@"; do
    if test -z "$tre_regex_t_field"; then
      AC_COMPILE_IFELSE(
        [ AC_LANG_PROGRAM([
#include <sys/types.h>
#include <regex.h>
      ],
      [
    regex_t foo;
    void *bar = foo.$field;
    foo.$field = bar;
      ])],
        [ AC_MSG_RESULT($field)
          tre_regex_t_field="$field" ])
    fi
  done
  if test -z "$tre_regex_t_field"; then
    AC_MSG_RESULT(no)
    tre_system_abi="no (unknown regex_t contents, report to \
Ville Laurikari <vl@iki.fi>)"
  fi
fi

# Check if we can redefine the functions...
if test "$tre_system_abi" = "yes"; then
  AC_DEFINE(_REGCOMP_INTERNAL, 1, [Define on IRIX])
  AC_MSG_CHECKING([if POSIX regex functions can be redefined])
  AC_COMPILE_IFELSE(
        [ AC_LANG_PROGRAM([
#include <sys/types.h>
#include <regex.h>

int
regcomp(regex_t *preg, const char *regex, int cflags)
{
  return 0;
}
void
regfree(regex_t *preg)
{
  return;
}
int
regexec(const regex_t *preg, const char *str,
        size_t nmatch, regmatch_t pmatch@<:@@:>@, int eflags)
{
  return 0;
}
size_t
regerror(int errcode, const regex_t *preg, char *errbuf,
	 size_t errbuf_size)
{
  return 0;
}
      ])],
        [ AC_MSG_RESULT(yes)
          tre_system_abi="yes" ],
        [ AC_MSG_RESULT(no)
          tre_system_abi="no (unable to redefine system functions)" ])
fi

if test "$tre_system_abi" = "yes"; then
  AC_CHECK_TYPES([reg_errcode_t],,,[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif /* HAVE_REGEX_H */
    ])

  AC_DEFINE(TRE_USE_SYSTEM_REGEX_H, 1,
    [Define to include the system regex.h from TRE regex.h])
  AC_DEFINE_UNQUOTED(TRE_SYSTEM_REGEX_H_PATH, "$tre_system_regex_h",
    [Define to the absolute path to the system regex.h])
  AC_DEFINE_UNQUOTED(TRE_REGEX_T_FIELD, $tre_regex_t_field,
    [Define to a field in the regex_t struct where TRE should store a pointer to
  the internal tre_tnfa_t structure])
else
  AC_DEFINE(TRE_REGEX_T_FIELD, value)
fi

AC_CHECK_FUNCS([isascii isblank])

dnl Check for wchar_t support
AC_ARG_ENABLE(wchar,
  AC_HELP_STRING(
    [--disable-wchar],
    [disable the wide character (wchar_t) support   @<:@default=detect@:>@]),
  [ tre_enable_wchar="$enableval" ],
  [ tre_enable_wchar="detect" ])

if test "$tre_enable_wchar" != "no"; then
  dnl We may need these headers
  AC_CHECK_HEADERS([wchar.h wctype.h])
  dnl We need wchar_t and WCHAR_MAX
  AC_CHECK_TYPES([wchar_t],
    [ tre_wchar="yes"
      VL_DECL_WCHAR_MAX ],
    [ tre_wchar_reason="wchar_t type not found"
      tre_wchar="no ($tre_wchar_reason)" ])
  if test "$tre_wchar" = "yes"; then
    dnl We may need mbstate_t
    AC_CHECK_TYPES([mbstate_t],,,[
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */
      ])
    found="no"
    dnl We need either wcsrtombs (preferred) or wcstombs
    AC_CHECK_FUNCS([wcsrtombs wcstombs],[found="yes"; break])
    if test "$found" = "no"; then
      tre_wchar_reason="no wcsrtombs or wcstombs found"
      tre_wchar="no ($tre_wchar_reason)"
    else
      dnl We need all these
      AC_CHECK_FUNCS([iswctype iswlower iswupper towlower towupper wcschr \
                      wcscpy wcslen wcsncpy wctype],
	[ tre_wchar="yes" ],
  	[ tre_wchar_reason="$ac_func not found"
          tre_wchar="no ($tre_wchar_reason)";
          break ])
    fi
  fi
else
  tre_wchar_reason="disabled with --disable-wchar"
  tre_wchar="no ($tre_wchar_reason)"
fi
if test "$tre_enable_wchar" = "yes"; then
  if test "$tre_wchar" != "yes"; then
    AC_MSG_ERROR([Cannot enable wide character support: $tre_wchar_reason])
  fi
fi

if test "$tre_wchar" = "yes"; then
  AC_DEFINE(TRE_WCHAR, 1,
    [Define to enable wide character (wchar_t) support.])
fi

dnl Check for multibyte character set support
AC_ARG_ENABLE(multibyte,
  AC_HELP_STRING(
    [--disable-multibyte],
    [disable multibyte character set support @<:@default=detect@:>@]),
  [ tre_enable_multibyte="$enableval" ],
  [ tre_enable_multibyte="detect" ])

dnl Wide character support is required for multibyte character set support
if test "$tre_wchar" != "yes"; then
  if test "$tre_enable_multibyte" = "yes"; then
    AC_MSG_ERROR([Cannot enable multibyte character support because wide \
character support is not enabled ($tre_wchar_reason)])
  fi
fi

if test "$tre_enable_multibyte" != "no"; then
  if test "$tre_wchar" != "yes"; then
    tre_multibyte="no (requires wide character support)"
  else
    found="no"
    dnl We need either mbrtowc (preferred) or mbtowc
    AC_CHECK_FUNCS([mbrtowc mbtowc],[found="yes"; break])
    if test "$found" = "no"; then
      tre_mbs_reason="no mbrtowc or mbtowc found"
      tre_multibyte="no ($tre_mbs_reason)"
    else
      tre_multibyte="yes"
    fi
  fi
else
  tre_multibyte="no (disabled with --disable-multibyte)"
fi
if test "$tre_enable_multibyte" = "yes"; then
  if test "$tre_multibyte" != "yes"; then
    AC_MSG_ERROR([Cannot enable multibyte character set support: $tre_mbs_reason])
  fi
fi

if test "$tre_multibyte" = "yes"; then
  AC_DEFINE(TRE_MULTIBYTE, 1,
    [Define to enable multibyte character set support.])
fi

AM_GNU_GETTEXT([external])
AC_PROG_LIBTOOL

dnl Output files
AC_CONFIG_HEADERS([config.h lib/tre-config.h])
AC_CONFIG_FILES([Makefile lib/Makefile src/Makefile tests/Makefile \
                 po/Makefile.in m4/Makefile utils/Makefile tre.pc])
AC_OUTPUT


dnl Print configuration summary

cat <<EOF


Configuration summary
=====================

TRE is now configured as follows:

* Compilation environment

  CC       = $CC
  CFLAGS   = $CFLAGS
  CPP      = $CPP
  CPPFLAGS = $CPPFLAGS
  LD       = $LD
  LDFLAGS  = $LDFLAGS

* TRE options

  Development-time debugging:              $tre_debug
  System regex ABI compatibility:          $tre_system_abi
  Wide character (wchar_t) support:        $tre_wchar
  Multibyte character set support:         $tre_multibyte
  Approximate matching support:            $tre_approx
  Build and install agrep:                 $tre_agrep

EOF
