## Process this file with automake to produce Makefile.in

if TRE_AGREP
agrep_dir = src
else
agrep_dir =
endif

SUBDIRS = lib $(agrep_dir) tests utils po m4

EXTRA_DIST = utils/config.rpath  \
	LICENSE \
	doc/tre-api.html \
	doc/tre-syntax.html \
	win32/tre-config.h win32/config.h \
	win32/tre.dsw \
	win32/tre.dsp win32/tre.def \
	win32/retest.dsp

ACLOCAL_AMFLAGS = -I m4
AC_CONFIG_AUX_DIR = utils

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tre.pc
