## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = retest bench randtest test-approx

retest_SOURCES = retest.c
retest_LDFLAGS = -static

test_approx_SOURCES = test-approx.c
test_approx_LDADD = ../lib/libtre.la -lm

randtest_SOURCES = randtest.c
randtest_LDFLAGS = -static

bench_SOURCES = bench.c
bench_LDADD = ../lib/libtre.la -lm
bench_LDFLAGS = -static

if TRE_DEBUG
  retest_LDADD = ../lib/libtre.la
  randtest_LDADD = ../lib/libtre.la
else !TRE_DEBUG
  ## Debugging is not turned on.  Make a malloc-debugging version of the
  ## library and use it with `retest' and `randtest'.  Malloc-debugging is
  ## not used for the debugging version since it would just get in the way.
  noinst_LTLIBRARIES = libxtre.la
  libxtre_la_SOURCES =			\
	../lib/tre-mem.c		\
	../lib/tre-compile.c		\
	../lib/tre-match-parallel.c	\
	../lib/tre-match-backtrack.c	\
	../lib/tre-match-approx.c	\
	../lib/regcomp.c		\
	../lib/regexec.c		\
	../lib/regerror.c		\
	../lib/xmalloc.c
  libxtre_la_CFLAGS = -DMALLOC_DEBUGGING

  retest_CFLAGS = -DMALLOC_DEBUGGING
  retest_LDADD = libxtre.la
  randtest_CFLAGS = -DMALLOC_DEBUGGING
  randtest_LDADD = libxtre.la
endif !TRE_DEBUG

INCLUDES = -I$(top_srcdir)/lib

TESTS = retest test-approx
