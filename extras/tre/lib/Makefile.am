## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libtre.la
libtre_la_LDFLAGS = -version-info 3:1:0

noinst_HEADERS =		\
	tre-internal.h		\
	tre-match-utils.h	\
	tre-mem.h		\
	xmalloc.h 		\
	gettext.h

libtre_la_SOURCES =		\
	tre-mem.c		\
	tre-match-parallel.c	\
	tre-match-backtrack.c	\
	tre-match-approx.c	\
	regcomp.c		\
	regexec.c		\
	regerror.c

libtre_la_LIBADD = $(LTLIBINTL)

dist_pkginclude_HEADERS = regex.h
nodist_pkginclude_HEADERS = tre-config.h
