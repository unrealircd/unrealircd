/*
  tre-match-backtrack.c - TRE backtracking regex matching engine

  Copyright (C) 2001-2003 Ville Laurikari <vl@iki.fi>.

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

/*
   This matcher is for regexps that use back referencing.  Regexp matching
   with back referencing is an NP-complete problem on the number of back
   references.  The easiest way to match them is to use a backtracking
   routine which basically goes through all possible paths in the TNFA
   and chooses the one which results in the best (leftmost and longest)
   match.  This can be spectacularly expensive and may run out of stack
   space, but there really is no better known generic algorithm.  Quoting
   Henry Spencer from comp.compilers:
   <URL: http://compilers.iecc.com/comparch/article/93-03-102>

     POSIX.2 REs require longest match, which is really exciting to
     implement since the obsolete ("basic") variant also includes
     \<digit>.  I haven't found a better way of tackling this than doing
     a preliminary match using a DFA (or simulation) on a modified RE
     that just replicates subREs for \<digit>, and then doing a
     backtracking match to determine whether the subRE matches were
     right.  This can be rather slow, but I console myself with the
     thought that people who use \<digit> deserve very slow execution.
     (Pun unintentional but very appropriate.)

*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif /* HAVE_WCTYPE_H */
#ifndef TRE_WCHAR
#include <ctype.h>
#endif /* !TRE_WCHAR */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */

#include "tre-internal.h"
#include "tre-mem.h"
#include "tre-match-utils.h"
#include "regex.h"
#include "xmalloc.h"

typedef struct {
  int pos;
  const char *str_byte;
#ifdef TRE_WCHAR
  const wchar_t *str_wide;
#endif /* TRE_WCHAR */
  tre_tnfa_transition_t *state;
  int next_c;
  int *tags;
#ifdef TRE_MBSTATE
  mbstate_t mbstate;
#endif /* TRE_MBSTATE */
} tre_backtrack_item_t;

typedef struct tre_backtrack_struct {
  tre_backtrack_item_t item;
  struct tre_backtrack_struct *prev;
  struct tre_backtrack_struct *next;
} *tre_backtrack_t;

#ifdef TRE_WHAR
#define BT_STACK_WIDE_IN     stack->item.str_wide = (_str_wide)
#define BT_STACK_WIDE_OUT    (_str_wide) = stack->item.str_wide
#else /* !TRE_WCHAR */
#define BT_STACK_WIDE_IN
#define BT_STACK_WIDE_OUT
#endif /* !TRE_WCHAR */

#ifdef TRE_MBSTATE
#define BT_STACK_MBSTATE_IN  stack->item.mbstate = (mbstate)
#define BT_STACK_MBSTATE_OUT (mbstate) = stack->item.mbstate
#else /* !TRE_MBSTATE */
#define BT_STACK_MBSTATE_IN
#define BT_STACK_MBSTATE_OUT
#endif /* !TRE_MBSTATE */

#define BT_STACK_PUSH(_pos, _str_byte, _str_wide, _state, _next_c, _tags, _mbstate) \
  do									      \
    {									      \
      int i;								      \
      if (!stack->next)					                      \
	{								      \
          tre_backtrack_t s;                                                  \
	  s = tre_mem_alloca(mem, sizeof(*s));	                              \
	  if (!s)						              \
	    return REG_ESPACE;						      \
	  s->prev = stack;					              \
	  s->next = NULL;					              \
	  s->item.tags = tre_mem_alloca(mem, sizeof(*tags) * tnfa->num_tags); \
	  if (!s->item.tags)	       			                      \
	    return REG_ESPACE;						      \
          stack->next = s;                                                    \
          stack = s;                                                          \
	}                                                                     \
      else                                                                    \
	stack = stack->next;						      \
      stack->item.pos = (_pos);						      \
      stack->item.str_byte = (_str_byte);      				      \
      BT_STACK_WIDE_IN;                                                       \
      stack->item.state = (_state);    		 		              \
      stack->item.next_c = (_next_c);					      \
      for (i = 0; i < tnfa->num_tags; i++)				      \
	stack->item.tags[i] = (_tags)[i];      				      \
      BT_STACK_MBSTATE_IN;                                                    \
    }									      \
  while (0)

#define BT_STACK_POP()                                                        \
  do									      \
    {                                                                         \
      int i;								      \
      assert(stack->prev);                                                    \
      pos = stack->item.pos;						      \
      str_byte = stack->item.str_byte;					      \
      BT_STACK_WIDE_OUT;						      \
      state = stack->item.state;					      \
      next_c = stack->item.next_c;					      \
      for (i = 0; i < tnfa->num_tags; i++)				      \
        tags[i] = stack->item.tags[i];					      \
      BT_STACK_MBSTATE_OUT;						      \
      stack = stack->prev;						      \
    }                                                                         \
  while (0)

#undef MIN
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

reg_errcode_t
tre_tnfa_run_backtrack(const tre_tnfa_t *tnfa, const void *string,
		       int len, tre_str_type_t type, int *match_tags,
		       int eflags, int *match_end_ofs)
{
  int pos = 0, pos_start = -1, pos_add_next = 1;
  int match_eo = -1;
  int *next_tags, *tags;
  tre_tnfa_transition_t *trans_i;
  tre_tnfa_transition_t *state;
  tre_char_t prev_c = 0, next_c_start, next_c = 0;
  const char *str_byte = string;
  const char *str_byte_start;
  int cflags = tnfa->cflags;
  int reg_notbol = eflags & REG_NOTBOL;
  int reg_noteol = eflags & REG_NOTEOL;
  int reg_newline = cflags & REG_NEWLINE;
  regmatch_t *pmatch;
  tre_mem_t mem = tre_mem_newa();
  tre_backtrack_t stack;
#ifdef TRE_WCHAR
  const wchar_t *str_wide = string;
  const wchar_t *str_wide_start;
#ifdef TRE_MBSTATE
  mbstate_t mbstate, mbstate_start;
  memset(&mbstate, '\0', sizeof(mbstate));
#endif /* TRE_MBSTATE */
#endif /* TRE_WCHAR */

  if (!mem)
    return REG_ESPACE;
  stack = tre_mem_alloca(mem, sizeof(*stack));
  if (!stack)
    return REG_ESPACE;
  stack->prev = NULL;
  stack->next = NULL;

  DPRINT(("tnfa_execute_backtrack, input type %d\n", type));
  DPRINT(("len = %d\n", len));

  tags = alloca(sizeof(*tags) * tnfa->num_tags);
  pmatch = alloca(sizeof(*pmatch) * tnfa->num_submatches);

 retry:
  {
    int i;
    for (i = 0; i < tnfa->num_tags; i++)
      tags[i] = match_tags[i] = -1;
  }

  state = NULL;
  pos = pos_start;
  GET_NEXT_WCHAR();
  pos_start = pos;
  next_c_start = next_c;
  str_byte_start = str_byte;
#ifdef TRE_WCHAR
  str_wide_start = str_wide;
#endif /* TRE_WCHAR */
#ifdef TRE_MBSTATE
  mbstate_start = mbstate;
#endif /* TRE_MBSTATE */

  /* Handle initial states. */
  next_tags = NULL;
  for (trans_i = tnfa->initial; trans_i->state; trans_i++)
    {
      DPRINT(("> init %p, prev_c %lc\n", trans_i->state, (wint_t)prev_c));
      if (trans_i->assertions && CHECK_ASSERTIONS(trans_i->assertions))
	{
	  DPRINT(("assert failed\n"));
	  continue;
	}
      if (state == NULL)
	{
	  /* Start from this state. */
	  state = trans_i->state;
	  next_tags = trans_i->tags;
	}
      else
	{
	  /* Backtrack to this state. */
	  DPRINT(("saving state %p for backtracking\n", trans_i->state));
	  BT_STACK_PUSH(pos, str_byte, str_wide, trans_i->state, next_c,
			tags, mbstate);
	  {
	    int *tmp = trans_i->tags;
	    if (tmp)
	      while (*tmp >= 0)
		stack->item.tags[*tmp++] = pos;
	  }
	}
    }

  if (next_tags)
    for (; *next_tags >= 0; next_tags++)
      tags[*next_tags] = pos;


  DPRINT(("entering match loop, pos %d, str_byte %p\n", pos, str_byte));
  DPRINT(("pos:chr/code | state and tags\n"));
  DPRINT(("-------------+------------------------------------------------\n"));

  if (state == NULL)
    goto backtrack;

  while (1)
    {
      tre_tnfa_transition_t *trans_i, *next_state;

      DPRINT(("start loop\n"));
      if (state == tnfa->final)
	{
	  DPRINT(("  match found, %d %d\n", match_eo, pos));
	  if (match_eo < pos
	      || (match_eo == pos
		  && tag_order(tnfa->num_tags, tnfa->tag_directions,
			       tags, match_tags)))
	    {
	      int i;
	      /* This match wins the previous match. */
	      DPRINT(("  win previous\n"));
	      match_eo = pos;
	      for (i = 0; i < tnfa->num_tags; i++)
		match_tags[i] = tags[i];
	    }
	  /* Our TNFAs never have transitions leaving from the final state,
	     so we jump right to backtracking. */
	  goto backtrack;
	}

#ifdef TRE_DEBUG
      DPRINT(("%3d:%2lc/%05d | %p ", pos, (tre_cint_t)next_c, (int)next_c,
	      state));
      {
	int i;
	for (i = 0; i < tnfa->num_tags; i++)
	  DPRINT(("%d%s", tags[i], i < tnfa->num_tags - 1 ? ", " : ""));
	DPRINT(("\n"));
      }
#endif /* TRE_DEBUG */

      /* Go to the next character in the input string. */
      trans_i = state;
      if (trans_i->state && trans_i->assertions & ASSERT_BACKREF)
	{
	  /* This is a back reference state.  All transitions leaving from
	     this state have the same back reference "assertion".  Instead
	     of reading the next character, we match the back reference. */
	  int so, eo, bt = trans_i->u.backref;
	  int bt_len;
	  int result;
	  DPRINT(("  should match back reference %d\n", bt));
	  /* Get the substring we need to match against. */
	  tre_fill_pmatch(bt + 1, pmatch, tnfa, tags, pos);
	  so = pmatch[bt].rm_so;
	  eo = pmatch[bt].rm_eo;
	  bt_len = eo - so;
	  /* XXX - implement for STR_WIDE */
	  DPRINT(("  substring (len %d) is [%d, %d[: '%.*s'\n",
		  bt_len, so, eo, bt_len, (char*)string + so));
#ifdef TRE_DEBUG
	  {
	    int slen;
	    if (len < 0)
	      slen = bt_len;
	    else
	      slen = MIN(bt_len, len - pos);
	    DPRINT(("  current string is '%.*s'\n", slen, str_byte - 1));
	  }
#endif

	  if (len < 0)
	    result = strncmp((char*)string + so, str_byte - 1, bt_len);
	  else if (len - pos < bt_len)
	    result = 1;
	  else
	    result = memcmp((char*)string + so, str_byte - 1, bt_len);

	  /* We can ignore multibyte characters here because the backref
	     string is already aligned at character boundaries. */
	  if (result == 0)
	    {
	      /* Back reference matched.  Advance in input string and resync
		 `prev_c', `next_c' and pos. */
	      DPRINT(("  back reference matched\n"));
	      str_byte += bt_len - 1;
	      pos += bt_len - 1;
	      GET_NEXT_WCHAR();
	      DPRINT(("  pos now %d\n", pos));
	    }
	  else
	    {
	      DPRINT(("  back reference did not match\n"));
	      goto backtrack;
	    }
	}
      else
	{
	  /* Read the next character. */
	  GET_NEXT_WCHAR();

	  /* Check for end of string. */
	  if (len < 0)
	    {
	      if (prev_c == L'\0')
		goto backtrack;
	    }
	  else
	    {
	      if (pos > len)
		goto backtrack;
	    }
	}

      next_state = NULL;
      for (trans_i = state; trans_i->state; trans_i++)
	{
	  DPRINT(("  transition %d-%d (%c-%c) %d to %p\n",
		  trans_i->code_min, trans_i->code_max,
		  trans_i->code_min, trans_i->code_max,
		  trans_i->assertions, trans_i->state));
	  if (trans_i->code_min <= prev_c && trans_i->code_max >= prev_c)
	    {
	      if (trans_i->assertions
		  && (CHECK_ASSERTIONS(trans_i->assertions)
		      /* Handle character class transitions. */
		      || ((trans_i->assertions & ASSERT_CHAR_CLASS)
			  && !(cflags & REG_ICASE)
			  && !tre_isctype((tre_cint_t)prev_c, trans_i->u.class))
		      || ((trans_i->assertions & ASSERT_CHAR_CLASS)
			  && (cflags & REG_ICASE)
			  && (!tre_isctype(tre_tolower((tre_cint_t)prev_c),
					   trans_i->u.class)
			      && !tre_isctype(tre_toupper((tre_cint_t)prev_c),
					      trans_i->u.class)))
		      || ((trans_i->assertions & ASSERT_CHAR_CLASS_NEG)
			  && neg_char_classes_match(trans_i->neg_classes,
						    (tre_cint_t)prev_c,
						    cflags & REG_ICASE))))
		{
		  DPRINT(("  assertion failed\n"));
		  continue;
		}

	      if (next_state == NULL)
		{
		  /* First matching transition. */
		  DPRINT(("  Next state is %p\n", trans_i->state));
		  next_state = trans_i->state;
		  next_tags = trans_i->tags;
		}
	      else
		{
		  /* Second mathing transition.  We may need to backtrack here
		     to take this transition instead of the first one, so we
		     push this transition in the backtracking stack so we can
		     jump back here if needed. */
		  DPRINT(("  saving state %p for backtracking\n",
			  trans_i->state));
		  BT_STACK_PUSH(pos, str_byte, str_wide, trans_i->state,
				next_c, tags, mbstate);
		  {
		    int *tmp;
		    for (tmp = trans_i->tags; tmp && *tmp >= 0; tmp++)
		      stack->item.tags[*tmp] = pos;
		  }
#if 0 /* XXX - it's important not to look at all transitions here to keep
	 the stack small! */
		  break;
#endif
		}
	    }
	}

      if (next_state != NULL)
	{
	  /* Matching transitions were found.  Take the first one. */
	  state = next_state;

	  /* Update the tag values. */
	  if (next_tags)
	    while (*next_tags >= 0)
	      tags[*next_tags++] = pos;
	}
      else
	{
	backtrack:
	  /* A matching transition was not found.  Try to backtrack. */
	  if (stack->prev)
	    {
	      DPRINT(("  backtracking\n"));
	      BT_STACK_POP();
	    }
	  else if (match_eo < 0)
	    {
	      /* Try starting from a later position in the input string. */
	      /* Check for end of string. */
	      if (len < 0)
		{
		  if (next_c == L'\0')
		    {
		      DPRINT(("end of string.\n"));
		      break;
		    }
		}
	      else
		{
		  if (pos >= len)
		    {
		      DPRINT(("end of string.\n"));
		      break;
		    }
		}
	      DPRINT(("restarting from next start position\n"));
	      next_c = next_c_start;
#ifdef TRE_MBSTATE
	      mbstate = mbstate_start;
#endif /* TRE_MBSTATE */
	      str_byte = str_byte_start;
#ifdef TRE_WCHAR
	      str_wide = str_wide_start;
#endif /* TRE_WCHAR */
	      goto retry;
	    }
	  else
	    {
	      DPRINT(("finished\n"));
	      break;
	    }
	}
    }

  *match_end_ofs = match_eo;
  return match_eo >= 0 ? REG_OK : REG_NOMATCH;
}
