/*
  test-approx.c - TRE approximate matching tests

  Copyright (C) 2003 Ville Laurikari <vl@iki.fi>.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License version 2 (June
  1991) as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <locale.h>
#include <string.h>
#if HAVE_RX
#include <hackerlab/rx-posix/regex.h>
#else
#include <regex.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */

#ifdef MALLOC_DEBUGGING
#include "xmalloc.h"
#endif /* MALLOC_DEBUGGING */

#define elementsof(x)   (sizeof(x)/sizeof(x[0]))

static int valid_reobj = 0;
static regex_t reobj;
static regmatch_t pmatch[32];
static char *regex_pattern;
static int cflags;

static int comp_tests = 0;
static int exec_tests = 0;
static int comp_errors = 0;
static int exec_errors = 0;

#ifndef REG_OK
#define REG_OK 0
#endif /* REG_OK */

#define END -2

void
test_exec(char *str, int cost_ins, int cost_del, int cost_subst,
	  int max_cost, int eflags, int should_match, int cost, ...)
{
  int i;
  int m;
  int rm_so, rm_eo;
  int fail = 0;
  char *data;
  regamatch_t match;
  regaparams_t params;
  va_list ap;
  va_start(ap, cost);
  match.pmatch = pmatch;
  match.nmatch = elementsof(pmatch);
  memset(&params, 0, sizeof(params));
  params.cost_ins = cost_ins;
  params.cost_del = cost_del;
  params.cost_subst = cost_subst;
  params.max_cost = max_cost;

  exec_tests++;

  if (!valid_reobj)
    {
      exec_errors++;
      return;
    }

  data = malloc(strlen(str));
  strncpy(data, str, strlen(str));
  m = reganexec(&reobj, data, strlen(str), &match, params, eflags);
  if (m != should_match)
    {
      printf("Exec error, regex: \"%s\", cflags %d, "
	     "string: \"%s\", eflags %d\n", regex_pattern, cflags,
	     str, eflags);
      printf("  got %smatch\n", m ? "no " : "");
      fail = 1;
    }

  if (!fail && m == 0)
    {
      if (match.cost != cost)
	{
	  printf("Exec error, regex: \"%s\", string: \"%s\"\n",
		 regex_pattern, str);
	  printf("  expected match cost %d, got %d\n", cost, match.cost);
	}

      for (i = 0; i < elementsof(pmatch); i++)
	{
	  rm_so = va_arg(ap, int);
	  if (rm_so == END)
	    break;
	  rm_eo = va_arg(ap, int);
	  if (pmatch[i].rm_so != rm_so
	      || pmatch[i].rm_eo != rm_eo)
	    {
	      printf("Exec error, regex: \"%s\", string: \"%s\"\n",
		     regex_pattern, str);
	      printf("  group %d: expected (%d, %d), got (%d, %d)\n",
		     i, rm_so, rm_eo, pmatch[i].rm_so, pmatch[i].rm_eo);
	      fail = 1;
	    }
	}

      va_end(ap);
      if (!(cflags & REG_NOSUB) && reobj.re_nsub != i - 1
	  && reobj.re_nsub <= elementsof(pmatch))
	{
	  printf("Comp error, regex: \"%s\"\n", regex_pattern);
	  printf("  re_nsub is %d, should be %d\n", reobj.re_nsub, i - 1);
	  fail = 1;
	}


      for (; i < elementsof(pmatch); i++)
	if (pmatch[i].rm_so != -1 || pmatch[i].rm_eo != -1)
	  {
	    if (!fail)
	      printf("Exec error, regex: \"%s\", string: \"%s\"\n",
		     regex_pattern, str);
	    printf("  group %d: expected (-1, -1), got (%d, %d)\n",
		   i, pmatch[i].rm_so, pmatch[i].rm_eo);
	    fail = 1;
	  }
    }

  if (fail)
    exec_errors++;
}

void
test_comp(char *re, int flags, int ret)
{
  int errcode = 0;
  regex_pattern = re;
  cflags = flags;

  comp_tests++;

  if (valid_reobj)
    {
      regfree(&reobj);
      valid_reobj = 0;
    }

  errcode = regcomp(&reobj, regex_pattern, flags);

  if (errcode != ret)
    {
      printf("Comp error, regex: \"%s\"\n", regex_pattern);
      printf("  expected return code %d, got %d.\n",
	     ret, errcode);
      comp_errors++;
    }

  if (errcode == 0)
    valid_reobj = 1;
}


int
main(int argc, char **argv)
{
  test_comp("abc", REG_EXTENDED, 0);
  test_exec("xaxcxxxx", 1, 1, 1, 3, 0, REG_OK, 1, 1, 4, END);

  test_comp("ab", REG_EXTENDED, 0);
  test_exec("xa", 1, 1, 1, 2, 0, REG_OK, 1, 1, 2, END);

  test_comp("foobar", REG_EXTENDED, 0);
  test_exec("kstnu ksntuphr ksenti ksnte ksntoksuryont funbar "
	    "kosnteksntaik tkosnt "
	    "eksntoöky reksont ksntoekuph snlsknt",
	    1, 1, 1, 6, 0, REG_OK, 2, 42, 48, END);

  test_comp("reksont   ksntoekuph", REG_EXTENDED, 0);
  test_exec("kstnu ksntuphr ksenti ksnte ksntoksuryont funbar "
	    "kosnteksntaik tkosnt "
	    "eksntoöky reksont ksntoekuph snlsknt",
	    1, 1, 1, 20, 0, REG_OK, 2, 80, 98, END);

  test_comp("foo(bar)", REG_EXTENDED, 0);
  /* no errors. */
  test_exec("aaa foobar zzz", 1, 1, 1, 1, 0, REG_OK, 0, 4, 10, 7, 10, END);
  /* missing "b" */
  test_exec("aaa fooar zzz", 1, 1, 1, 1, 0, REG_OK, 1, 4, 9, 7, 9, END);
  /* extra "x" */
  test_exec("aaa fooxbar zzz", 1, 1, 1, 1, 0, REG_OK, 1, 4, 11, 7, 11, END);
  /* b changed to "d" */
  test_exec("aaa foodar zzz", 1, 1, 1, 1, 0, REG_OK, 1, 4, 10, 7, 10, END);

  test_comp("foobar", REG_EXTENDED, 0);
  test_exec("faobar\n", 1, 1, 1, 2, 0, REG_OK, 1, 0, 6, END);

  return 0;
}
