/*
  tre-match-approx.c - TRE approximate regex matching engine

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
  This algorithm is very similar to the one in `tre-match-parallel.c'.
  The main difference is that the input string does not need to match
  exactly to the language of the TNFA.  Each missing, extra, or
  changed character increases the "cost" of a match.  A maximum
  allowed cost must be specified.  If the maximum cost is set to zero,
  this matcher should return results identical to the exact matchers.
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
#include "tre-match-utils.h"
#include "regex.h"
#include "xmalloc.h"



typedef struct {
  tre_tnfa_transition_t *state;
  int cost;
  int *tags;
  int state_id;  /* XXX - this could be removed by using tre_tnfa_state_t
		    for `state'. */
} tre_tnfa_approx_reach_t;

typedef struct {
  int pos;
  int cost;
  tre_tnfa_approx_reach_t *reach;
  int **tags;
} tre_approx_reach_pos_t;


#ifdef TRE_DEBUG
static void
print_reach(const tre_tnfa_t *tnfa, tre_tnfa_approx_reach_t *reach)
{
  int i;

  while (reach->state != NULL)
    {
      DPRINT((" %p(%d)", (void *)reach->state, reach->cost));
      if (tnfa->num_tags > 0)
	{
	  DPRINT(("/"));
	  for (i = 0; i < tnfa->num_tags; i++)
	    {
	      DPRINT(("%d:%d", i, reach->tags[i]));
	      if (i < (tnfa->num_tags-1))
		DPRINT((","));
	    }
	}
      reach++;
    }
  DPRINT(("\n"));

}
#endif /* TRE_DEBUG */

reg_errcode_t
tre_tnfa_run_approx(const tre_tnfa_t *tnfa, const void *string, int len,
		    tre_str_type_t type, int *match_tags,
		    regamatch_t *match, regaparams_t params,
		    int eflags, int *match_end_ofs)
{
  tre_char_t prev_c = 0, next_c = 0;
  const char *str_byte = string;
  char *buf;
  int pos = -1, pos_add_next = 1;
  tre_tnfa_transition_t *trans_i;
  tre_tnfa_approx_reach_t *reach, *reach_next, *reach_i, *reach_next_i;
  tre_approx_reach_pos_t *reach_pos;
  int i;
  int reg_notbol = eflags & REG_NOTBOL;
  int reg_noteol = eflags & REG_NOTEOL;
  int reg_newline = tnfa->cflags & REG_NEWLINE;
  int match_eo = -1;       /* end offset of match (-1 if no match found yet) */
  int match_cost = -1;
  int cflags = tnfa->cflags;
  int *tmp_tags = NULL;
  int num_tags;
  int *tag_i;
  int *tmp_iptr;
#ifdef TRE_WCHAR
  const wchar_t *str_wide = string;
#ifdef TRE_MBSTATE
  mbstate_t mbstate;
  memset(&mbstate, '\0', sizeof(mbstate));
#endif /* TRE_MBSTATE */
#endif /* !TRE_WCHAR */

  DPRINT(("tre_tnfa_run_approx, input type %d\n", type));

  if (eflags & REG_NOTAGS)
    num_tags = 0;
  else
    num_tags = tnfa->num_tags;

  /* Allocate memory for temporary data required for matching.  This needs to
     be done for every matching operation to be thread safe.  This allocates
     everything in a single large block from the stack frame using alloca(). */
  {
    int tbytes, rbytes, pbytes, xbytes, total_bytes;
    char *tmp_buf;
    /* Compute the length of the block we need. */
    tbytes = sizeof(*tmp_tags) * num_tags;
    rbytes = sizeof(*reach_next) * (tnfa->num_states + 1);
    pbytes = sizeof(*reach_pos) * tnfa->num_states;
    xbytes = sizeof(int) * num_tags;
    total_bytes =
      (sizeof(long) - 1) * 4 /* for alignment paddings */
      + (rbytes + xbytes * tnfa->num_states) * 2 + tbytes + pbytes;

    /* Allocate the memory. */
    buf = alloca(total_bytes);
    if (buf == NULL)
      return REG_ESPACE;
    memset(buf, 0, total_bytes);

    /* Get the various pointers within tmp_buf (properly aligned). */
    tmp_tags = (void *)buf;
    tmp_buf = buf + tbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    reach_next = (void *)tmp_buf;
    tmp_buf += rbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    reach = (void *)tmp_buf;
    tmp_buf += rbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    reach_pos = (void *)tmp_buf;
    tmp_buf += pbytes;
    tmp_buf += ALIGN(tmp_buf, long);
    for (i = 0; i < tnfa->num_states; i++)
      {
	reach[i].tags = (void *)tmp_buf;
	tmp_buf += xbytes;
	reach_next[i].tags = (void *)tmp_buf;
	tmp_buf += xbytes;
	reach[i].cost = 0;
	reach_next[i].cost = 0;
      }
  }

  for (i = 0; i < tnfa->num_states; i++)
    reach_pos[i].pos = -1;

  reach_next_i = reach_next;
  GET_NEXT_WCHAR();

  DPRINT(("length: %d\n", len));
  DPRINT(("pos:chr/code | states and costs\n"));
  DPRINT(("-------------+------------------------------------------------\n"));

  while (1)
    {
      DPRINT(("pos %d\n", pos));
      /* Add the initial states to `reach_pos'. */
      if (match_eo < 0 || (match_eo >= 0 && match_cost > 0))
	{
	  DPRINT((" init >"));
	  trans_i = tnfa->initial;
	  while (trans_i->state != NULL)
	    {
	      if (reach_pos[trans_i->state_id].pos < pos)
		{
		  if (trans_i->assertions
		      && CHECK_ASSERTIONS(trans_i->assertions))
		    {
		      DPRINT(("assertion failed\n"));
		      trans_i++;
		      continue;
		    }

		  DPRINT((" %p", (void *)trans_i->state));
		  reach_next_i->state = trans_i->state;
		  for (i = 0; i < num_tags; i++)
		    reach_next_i->tags[i] = -1;
		  if (trans_i->tags)
		    for (tag_i = trans_i->tags; *tag_i >= 0; tag_i++)
		      reach_next_i->tags[*tag_i] = pos;
		  if (reach_next_i->state == tnfa->final)
		    {
		      match_eo = pos;
		      for (i = 0; i < num_tags; i++)
			match_tags[i] = reach_next_i->tags[i];
		    }
		  reach_next_i->cost = 0;
		  reach_next_i->state_id = trans_i->state_id;
		  DPRINT(("1: setting state %d pos to %d\n",
			  trans_i->state_id, pos));
		  reach_pos[trans_i->state_id].pos = pos;
		  reach_pos[trans_i->state_id].tags = &reach_next_i->tags;
		  reach_pos[trans_i->state_id].cost = 0;
		  reach_pos[trans_i->state_id].reach = reach_next_i;
		  reach_next_i++;
		}
	      trans_i++;
	    }
	  DPRINT(("\n"));
	  reach_next_i->state = NULL;
	}
      else if (reach_next_i == reach_next)
	{
	  DPRINT(("Found exact match.\n"));
	  break;
	}

      /* Handle insertions -- this is done by pretending there is an
	 epsilon transition from each state back to the same state
	 with cost `cost_ins'. */
      for (reach_i = reach; reach_i->state; reach_i++)
	{
	  int state_id = reach_i->state_id;
	  int cost = reach_i->cost + params.cost_ins;
	  if (cost > params.max_cost)
	    break;
	  if (reach_pos[state_id].pos < pos)
	    {
	      DPRINT(("insertion (new): state %p, cost %d\n",
		      reach_i->state, cost));
	      reach_next_i->cost = cost;
	      reach_next_i->state = reach_i->state;
	      reach_next_i->state_id = state_id;
	      for (i = 0; i < num_tags; i++)
		reach_next_i->tags[i] = reach_i->tags[i];
	      DPRINT(("2: setting state %d pos to %d\n",
		      trans_i->state_id, pos));
	      reach_pos[state_id].pos = pos;
	      reach_pos[state_id].tags = &reach_i->tags;
	      reach_pos[state_id].cost = cost;
	      reach_pos[state_id].reach = reach_next_i;
	      reach_next_i++;
	    }
	  else if (reach_pos[state_id].cost > cost)
	    {
	      DPRINT(("insertion (win): state %p, cost %d\n",
		      reach_i->state, cost));
	      reach_pos[state_id].cost = cost;
	      reach_pos[state_id].reach->cost = cost;
	    }
	}

      /* Handle deletions -- this is done by traversing through the whole
	 TNFA pretending that all transitions are epsilon transitions with
	 cost `cost_del'. */
      DPRINT(("deletions\n"));
      {
	/* XXX - how big should the ring buffer be? */
	tre_tnfa_approx_reach_t ringbuffer[512];
	tre_tnfa_approx_reach_t *deque_start, *deque_end;

	deque_start = deque_end = ringbuffer;

	/* Add all states in `reach_next' to the deque. */
	for (reach_i = reach_next; reach_i->state; reach_i++)
	  {
	    DPRINT(("adding %p to deque\n", reach_i->state));
	    deque_end->state = reach_i->state;
	    deque_end->cost = reach_i->cost;
	    deque_end->tags = reach_i->tags;
	    deque_end++;
	  }

	/* Repeat until the deque is empty */
	DPRINT(("deque has %d items\n", deque_end - deque_start));
	while (deque_end != deque_start)
	  {
	    tre_tnfa_transition_t *trans_i;
	    tre_tnfa_approx_reach_t *reach_p;
	    int cost;

	    /* Pop the first state off the deque. */
	    reach_p = deque_start;
	    cost = reach_p->cost + params.cost_del;
	    DPRINT(("deletion: from %p: ", reach_p->state));
	    if (cost > params.max_cost)
	      {
		DPRINT(("not possible\n"));
		deque_start++;
		if (deque_start >= (ringbuffer + 512))
		  deque_start = ringbuffer;
		continue;
	      }

	    for (trans_i = reach_p->state; trans_i->state; trans_i++)
	      {
		/* Compute the tags after this transition. */
		for (i = 0; i < num_tags; i++)
		  tmp_tags[i] = reach_p->tags[i];
		if (trans_i->tags)
		  for (tag_i = trans_i->tags; *tag_i >= 0; tag_i++)
		    tmp_tags[*tag_i] = pos;

		DPRINT(("to %p\n", trans_i->state));
		DPRINT(("pos = %d\n", pos));
		DPRINT(("cost = %d\n", cost));
		DPRINT(("reach_pos[%d].pos = %d\n", trans_i->state_id,
			reach_pos[trans_i->state_id].pos));
		DPRINT(("reach_pos[%d].cost = %d\n", trans_i->state_id,
			reach_pos[trans_i->state_id].cost));
		/* XXX - should use input order counts to optimize! */
		/* XXX - should we check some assertions (BOL, EOL)? */
		if (reach_pos[trans_i->state_id].pos < pos
		    || reach_pos[trans_i->state_id].cost > cost)
		  {
#ifdef TRE_DEBUG
		    DPRINT(("to %p, cost %d, tags ",
			    reach_p->state, cost));
		    for (i = 0; i < num_tags; i++)
		      DPRINT(("%d, ", tmp_tags[i]));
		    DPRINT(("\n"));
		    if (trans_i->state == tnfa->final)
		      DPRINT(("final state!\n"));
#endif /* TRE_DEBUG */
		    if (reach_pos[trans_i->state_id].pos < pos)
		      {
			/* Not reached yet. */
			reach_next_i->state = trans_i->state;
			reach_next_i->state_id = trans_i->state_id;
			reach_next_i->cost = cost;
			DPRINT(("3: setting state %d pos to %d\n",
				trans_i->state_id, pos));
			reach_pos[trans_i->state_id].pos = pos;
			reach_pos[trans_i->state_id].tags = &reach_next_i->tags;
			reach_pos[trans_i->state_id].cost = cost;
			reach_pos[trans_i->state_id].reach = reach_next_i;

			tmp_iptr = reach_next_i->tags;
			reach_next_i->tags = tmp_tags;
			tmp_tags = tmp_iptr;

			if (reach_next_i->state == tnfa->final
			    && cost <= params.max_cost
			    && (match_eo == -1
				|| match_cost > cost
				|| (match_cost == cost
				    && (num_tags > 0
					&& reach_next_i->tags[0] <= match_tags[0]))))
			  {
			    DPRINT(("setting new match at %d, cost %d\n",
				    pos, cost));
			    match_eo = pos;
			    match_cost = cost;
			    for (i = 0; i < num_tags; i++)
			      match_tags[i] = reach_next_i->tags[i];
			  }

			reach_next_i++;
		      }
		    else
		      {
			/* Already reached.  Reset cost. */
			reach_pos[trans_i->state_id].reach->cost = cost;
			reach_pos[trans_i->state_id].reach->state =
			  trans_i->state;
			tmp_iptr = *reach_pos[trans_i->state_id].tags;
			*reach_pos[trans_i->state_id].tags = tmp_tags;
			if (trans_i->state == tnfa->final
			    && (match_eo == -1
				|| match_cost > cost
				|| (match_cost == cost
				    && (num_tags > 0
					&& tmp_tags[0] <= match_tags[0]))))
			  {
			    DPRINT(("setting better match at %d, cost %d\n",
				    pos, cost));
			    match_eo = pos;
			    match_cost = cost;
			    for (i = 0; i < num_tags; i++)
			      match_tags[i] = tmp_tags[i];
			  }
			tmp_tags = tmp_iptr;
		      }

		    /* Add to the end of the deque. */
		    deque_end->state = trans_i->state;
		    deque_end->cost = cost;
		    deque_end->tags = tmp_tags;
		    deque_end++;
		    if (deque_end >= (ringbuffer + 512))
		      deque_end = ringbuffer;
		    assert(deque_end != deque_start);
		  }
	      }
	    DPRINT(("\n"));

	    deque_start++;
	    if (deque_start >= (ringbuffer + 512))
	      deque_start = ringbuffer;
	  }
      }

      reach_next_i->state = NULL;
      GET_NEXT_WCHAR();

#ifdef TRE_DEBUG
      DPRINT(("%3d:%2lc/%05d |", pos - 1, (tre_cint_t)prev_c, (int)prev_c));
      print_reach(tnfa, reach_next);
#endif /* TRE_DEBUG */

      /* Check for end of string. */
      if ((len < 0 && prev_c == L'\0')
	  || (len >= 0 && pos > len))
	break;

      /* Swap `reach' and `reach_next'. */
      reach_i = reach;
      reach = reach_next;
      reach_next = reach_i;

      /* Go through each state in `reach' and the transitions to
	 other states. */
      reach_i = reach;
      reach_next_i = reach_next;
      while (reach_i->state != NULL)
	{
	  trans_i = reach_i->state;
	  while (trans_i->state != NULL)
	    {
	      int cost = reach_i->cost;
	      if (trans_i->code_min > prev_c ||
		  trans_i->code_max < prev_c)
		{
		  /* Handle substitutions.  The required character was not
		     in the string, so match it in place of whatever was
		     supposed to be there and increase cost by `cost_subst'. */
		  cost += params.cost_subst;
		  if (cost > params.max_cost)
		    {
		      trans_i++;
		      continue;
		    }
		  DPRINT(("substitution, from %p, cost %d\n",
			  trans_i->state, cost));
		}

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
		  DPRINT(("assertion failed\n"));
		  trans_i++;
		  continue;
		}

	      /* Compute the tags after this transition. */
	      for (i = 0; i < num_tags; i++)
		tmp_tags[i] = reach_i->tags[i];
	      if (trans_i->tags)
		for (tag_i = trans_i->tags; *tag_i >= 0; tag_i++)
		  tmp_tags[*tag_i] = pos;

	      if (reach_pos[trans_i->state_id].pos < pos)
		{
		  /* Found an unvisited node. */
		  reach_next_i->state = trans_i->state;
		  reach_next_i->state_id = trans_i->state_id;
		  reach_next_i->cost = cost;
		  tmp_iptr = reach_next_i->tags;
		  reach_next_i->tags = tmp_tags;
		  tmp_tags = tmp_iptr;
		  DPRINT(("4: setting state %d pos to %d\n",
			  trans_i->state_id, pos));
		  reach_pos[trans_i->state_id].pos = pos;
		  reach_pos[trans_i->state_id].tags = &reach_next_i->tags;
		  reach_pos[trans_i->state_id].cost = cost;
		  reach_pos[trans_i->state_id].reach = reach_next_i;

		  if (reach_next_i->state == tnfa->final
		      && cost <= params.max_cost
		      && (match_eo == -1
			  || match_cost > cost
			  || (match_cost == cost
			      && (num_tags > 0
				  && reach_next_i->tags[0] <= match_tags[0]))))
		    {
		      DPRINT(("setting new match at %d, cost %d\n", pos, cost));
		      match_eo = pos;
		      match_cost = cost;
		      for (i = 0; i < num_tags; i++)
			match_tags[i] = reach_next_i->tags[i];
		    }
		  reach_next_i++;

		}
	      else
		{
		  assert(reach_pos[trans_i->state_id].pos == pos);
		  /* Another path has also reached this state.  We choose the
		     one with the smallest cost. */
		  if (reach_pos[trans_i->state_id].cost > cost
		      || (reach_pos[trans_i->state_id].cost == cost
			  && tag_order(num_tags, tnfa->tag_directions, tmp_tags,
				       *reach_pos[trans_i->state_id].tags)))
		    {
		      /* The new path wins. */
		      reach_pos[trans_i->state_id].cost = cost;
		      reach_pos[trans_i->state_id].reach->cost = cost;
		      tmp_iptr = *reach_pos[trans_i->state_id].tags;
		      *reach_pos[trans_i->state_id].tags = tmp_tags;
		      if (trans_i->state == tnfa->final)
			{
			  DPRINT(("setting better match at %d, cost %d\n",
				  pos, cost));
			  match_eo = pos;
			  match_cost = cost;
			  for (i = 0; i < num_tags; i++)
			    match_tags[i] = tmp_tags[i];
			}
		      tmp_tags = tmp_iptr;
		    }
		}
	      trans_i++;
	    }
	  reach_i++;
	}
      reach_next_i->state = NULL;
    }

  DPRINT(("match end offset = %d, match cost = %d\n", match_eo, match_cost));
  match->cost = match_cost;
  *match_end_ofs = match_eo;
  return match_eo >= 0 ? REG_OK : REG_NOMATCH;
}

/* EOF */
