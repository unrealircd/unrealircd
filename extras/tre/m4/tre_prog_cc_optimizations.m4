dnl @synopsis TRE_PROG_CC_OPTIMIZATIONS
dnl
dnl Sets C compiler optimizations which have been found to give
dnl best results for TRE with this compiler and architecture.
dnl
dnl @version 1.2
dnl @author Ville Laurikari <vl@iki.fi>
dnl
AC_DEFUN([TRE_PROG_CC_OPTIMIZATIONS], [
  # Don't override if CFLAGS was already set.
  if test -z "$ac_env_CFLAGS_set"; then
    AC_MSG_CHECKING([for the best optimization flags])
    if test "$GCC" = "yes"; then
      # -pg and -fomit-frame-pointer are incompatible
      if echo $CFLAGS | grep -e -pg > /dev/null 2>&1; then
        tre_opt_omit_fp=""
      else
        tre_opt_omit_fp="-fomit-frame-pointer"
      fi
      case "$target" in
        i686-*-*-* )
          OPT_CFLAGS="-O1 $tre_opt_omit_fp"
          ;;
      esac
    fi
    AC_MSG_RESULT($OPT_CFLAGS)
    CFLAGS="$CFLAGS $OPT_CFLAGS"
  fi
])dnl
