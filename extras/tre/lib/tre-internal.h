/*
  tre-internal.h - TRE internal definitions

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

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */
#ifdef HAVE_WCTYPE_H

#include <wctype.h>
#endif /* HAVE_WCTYPE_H */

#include "regex.h"

#ifdef TRE_DEBUG
#include <stdio.h>
#define DPRINT(msg) do {printf msg; fflush(stdout);} while(0)
#else /* !TRE_DEBUG */
#define DPRINT(msg) do { } while(0)
#endif /* !TRE_DEBUG */

#if defined HAVE_MBRTOWC
#define tre_mbrtowc(pwc, s, n, ps) (mbrtowc((pwc), (s), (n), (ps)))
#elif defined HAVE_MBTOWC
#define tre_mbrtowc(pwc, s, n, ps) (mbtowc((pwc), (s), (n)))
#endif

#ifdef TRE_MULTIBYTE
#ifdef HAVE_MBSTATE_T
#define TRE_MBSTATE
#endif /* TRE_MULTIBYTE */
#endif /* HAVE_MBSTATE_T */

/* Define the character types and functions. */
#ifdef TRE_WCHAR
/* Wide characters. */
typedef wchar_t tre_char_t;
typedef wint_t tre_cint_t;
typedef wctype_t tre_ctype_t;
#define TRE_CHAR_MAX WCHAR_MAX
#ifdef TRE_MULTIBYTE
#define TRE_MB_CUR_MAX MB_CUR_MAX
#else /* !TRE_MULTIBYTE */
#define TRE_MB_CUR_MAX 1
#endif /* !TRE_MULTIBYTE */
#define tre_isctype iswctype
#define tre_isalnum iswalnum
#define tre_isdigit iswdigit
#define tre_islower iswlower
#define tre_isupper iswupper
#define tre_tolower towlower
#define tre_toupper towupper
#define tre_ctype   wctype
#define tre_strlen  wcslen
#else /* !TRE_WCHAR */
/* 8 bit characters. */
typedef unsigned char tre_char_t;
typedef short tre_cint_t;
typedef int (*tre_ctype_t)(tre_cint_t);
#define TRE_CHAR_MAX 255
#define TRE_MB_CUR_MAX 1
#define tre_isctype(c, type) ( (type)(c) )
int tre_isalnum(tre_cint_t c);
int tre_isdigit(tre_cint_t c);
int tre_islower(tre_cint_t c);
int tre_isupper(tre_cint_t c);
#define tre_tolower(c) (tre_cint_t)(tolower(c))
#define tre_toupper(c) (tre_cint_t)(toupper(c))
tre_ctype_t tre_ctype(const char *name);
#define tre_strlen  strlen
#endif /* !TRE_WCHAR */

#define REG_NOTAGS (REG_NOTEOL << 1)
typedef enum { STR_WIDE, STR_BYTE, STR_MBS } tre_str_type_t;

/* Returns number of bytes to add to (char *)ptr to make it
   properly aligned for the type. */
#define ALIGN(ptr, type) \
  ((((long)ptr) % sizeof(type)) \
   ? (sizeof(type) - (((long)ptr) % sizeof(type))) \
   : 0)

#undef MAX
#undef MIN
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))

/* TNFA transition definition.  Each transition has a range of accepted
   characters, pointer to the next state and the ID number of that state,
   a -1 -terminated array of tags, assertion bitmap, and character class
   assertions.  A TNFA state is an array of transitions, the terminator is
   a transition with state == NULL: */
typedef struct tnfa_transition tre_tnfa_transition_t;

struct tnfa_transition {
  tre_cint_t code_min;
  tre_cint_t code_max;
  tre_tnfa_transition_t *state;
  int state_id;
  int *tags;
  int assertions;
  union {
    tre_ctype_t class;
    int backref;
  } u;
  tre_ctype_t *neg_classes;
};


/* Assertions. */
#define ASSERT_AT_BOL             1   /* Beginning of line. */
#define ASSERT_AT_EOL             2   /* End of line. */
#define ASSERT_CHAR_CLASS         4   /* Character class in `class'. */
#define ASSERT_CHAR_CLASS_NEG     8   /* Character classes in `neg_classes'. */
#define ASSERT_AT_BOW            16   /* Beginning of word. */
#define ASSERT_AT_EOW            32   /* End of word. */
#define ASSERT_AT_WB             64   /* Word boundary. */
#define ASSERT_AT_WB_NEG        128   /* Not a word boundary. */
#define ASSERT_BACKREF          256   /* A back reference in `backref'. */
#define ASSERT_LAST             256

/* Tag directions. */
typedef enum {
  TRE_TAG_MINIMIZE = 0,
  TRE_TAG_MAXIMIZE = 1
} tre_tag_direction_t;


/* Instructions to compute submatch register values from tag values
   after a successful match.  */
struct tre_submatch_data {
  /* Tag that gives the value for rm_so (submatch start offset). */
  int so_tag;
  /* Tag that gives the value for rm_eo (submatch end offset). */
  int eo_tag;
  /* List of submatches this submatch is contained in. */
  int *parents;
};

typedef struct tre_submatch_data tre_submatch_data_t;


/* TNFA definition. */
typedef struct tnfa tre_tnfa_t;

struct tnfa {
  tre_tnfa_transition_t *transitions;
  unsigned int num_transitions;
  tre_tnfa_transition_t *initial;
  tre_tnfa_transition_t *final;
  tre_submatch_data_t *submatch_data;
  char *firstpos_chars;
  int first_char;
  unsigned int num_submatches;
  tre_tag_direction_t *tag_directions;
  int *minimal_tags;
  int *marker_offs;
  int num_tags;
  int num_minimals;
  int end_tag;
  int num_states;
  int cflags;
  int have_backrefs;
};

void
tre_fill_pmatch(size_t nmatch, regmatch_t pmatch[],
		const tre_tnfa_t *tnfa, int *tags, int match_eo);

reg_errcode_t
tre_tnfa_run_parallel(const tre_tnfa_t *tnfa, const void *string, int len,
		      tre_str_type_t type, int *match_tags, int eflags,
		      int *match_end_ofs);

reg_errcode_t
tre_tnfa_run_parallel(const tre_tnfa_t *tnfa, const void *string, int len,
		      tre_str_type_t type, int *match_tags, int eflags,
		      int *match_end_ofs);

reg_errcode_t
tre_tnfa_run_backtrack(const tre_tnfa_t *tnfa, const void *string,
		       int len, tre_str_type_t type, int *match_tags,
		       int eflags, int *match_end_ofs);

#ifdef TRE_APPROX
reg_errcode_t
tre_tnfa_run_approx(const tre_tnfa_t *tnfa, const void *string, int len,
		    tre_str_type_t type, int *match_tags,
		    regamatch_t *match, regaparams_t params,
		    int eflags, int *match_end_ofs);
#endif /* TRE_APPROX */
