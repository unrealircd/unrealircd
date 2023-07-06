/*
 *   Unreal Internet Relay Chat Daemon, src/match.c
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "unrealircd.h"

ID_Copyright("(C) 1990 Jarkko Oikarinen");

/*
 *  Compare if a given string (name) matches the given
 *  mask (which can contain wild cards: '*' - match any
 *  number of chars, '?' - match any single character.
 *
 *	return	0, if match
 *		1, if no match
 */

u_char touppertab[], tolowertab[];
#define tolowertab2 tolowertab
#define lc(x) tolowertab2[x]

/* Match routine for special cases where escaping is needed in a normal fashion.
 * Checks a string ('name') against a globbing(+more) pattern ('mask').
 * Original by Douglas A Lewis (dalewis@acsu.buffalo.edu).
 * Code based on hybrid7's version (match_esc()).
 * Various modifications by Bram Matthys (Syzop).
 * Returns 1 on match and 0 for no match.
 * Instead of our previous code, this one is less optimized but actually  _readable_ ;).
 * Modifications I (Syzop) had to do vs the hybrid7 code:
 * - Got rid of (u_char *) casts, since we already compile with
 *   chars defaulting to unsigned [or else major things break] ;).
 * - Support for '_'.
 * - Rip out support for '#'.
 */
int match_esc(const char *mask, const char *name)
{
	const u_char *m = mask;
	const u_char *n = name;
	const u_char *ma = NULL;
	const u_char *na = name;

	while(1)
	{
		if (*m == '*')
		{
			while (*m == '*') /* collapse.. */
				m++;
			ma = m; 
			na = n;
		}
		
		if (!*m)
		{
			if (!*n)
				return 1;
			if (!ma)
				return 0;
			for (m--; (m > (const u_char *)mask) && (*m == '?'); m--);
			if (*m == '*')
				return 1;
			m = ma;
			n = ++na;
		} else
		if (!*n)
		{
			while (*m == '*') /* collapse.. */
				m++;
			return (*m == 0);
		}
		
		if (*m != '?')
		{
			if (*m == '\\')
				if (!*++m)
					return 0; /* unfinished escape sequence */
			if ((lc(*m) != lc(*n)) && !((*m == '_') && (*n == ' ')))
			{
				if (!ma)
					return 0;
				m = ma;
				n = ++na;
			} else
			{
				m++;
				n++;
			}
		} else
		{
			m++;
			n++;
		}
	}
	return 0;
}

/** Same credit/copyright as match_esc() applies, except escaping removed.. ;p */
int match_simple(const char *mask, const char *name)
{
	const u_char *m = mask;
	const u_char *n = name;
	const u_char *ma = NULL;
	const u_char *na = name;

	while(1)
	{
		if (*m == '*')
		{
			while (*m == '*') /* collapse.. */
				m++;
			ma = m; 
			na = n;
		}
		
		if (!*m)
		{
			if (!*n)
				return 1;
			if (!ma)
				return 0;
			for (m--; (m > (const u_char *)mask) && (*m == '?'); m--);
			if (*m == '*')
				return 1;
			m = ma;
			n = ++na;
		} else
		if (!*n)
		{
			while (*m == '*') /* collapse.. */
				m++;
			return (*m == 0);
		}
		
		if ((lc(*m) != lc(*n)) && !((*m == '_') && (*n == ' ')) && (*m != '?'))
		{
			if (!ma)
				return 0;
			m = ma;
			n = ++na;
		} else
		{
			m++;
			n++;
		}
	}
	return 0;
}

/*
 * collapse a pattern string into minimal components.
 * This particular version is "in place", so that it changes the pattern
 * which is to be reduced to a "minimal" size.
 */
char *collapse(char *pattern)
{
	char *s;
	char *s1;
	char *t;

	s = pattern;

	if (BadPtr(pattern))
		return pattern;
	/*
	 * Collapse all \** into \*, \*[?]+\** into \*[?]+
	 */
	for (; *s; s++)
		if (*s == '\\')
		{
			if (!*(s + 1))
				break;
			else
				s++;
		}
		else if (*s == '*')
		{
			if (*(t = s1 = s + 1) == '*')
				while (*t == '*')
					t++;
			else if (*t == '?')
				for (t++, s1++; *t == '*' || *t == '?'; t++)
					if (*t == '?')
						*s1++ = *t;
			while ((*s1++ = *t++))
				;
		}
	return pattern;
}


/* Case insensitive comparison of two NULL terminated strings,
 * using the "IRC nick comparison" rules. Or, well, partially
 * anyway.
 * Should be used for NICK-related comparisons. And probably
 * not even then, since this does not deal with multibyte.
 * @returns 	 0, if s1 equal to s2
 *		<0, if s1 lexicographically less than s2
 *		>0, if s1 lexicographically greater than s2
 */
int  smycmp(const char *s1, const char *s2)
{
	u_char *str1;
	u_char *str2;
	int  res;

	str1 = (u_char *)s1;
	str2 = (u_char *)s2;

	while ((res = toupper(*str1) - toupper(*str2)) == 0)
	{
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}

u_char tolowertab[] = {
	0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
	0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '[', '\\', ']', '^',
	'_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

u_char touppertab[] = {
	0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
	0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
	0x5f,
	'`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '|', '}', '~',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

u_char char_atribs[] = {
/* 0-7 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 8-12 */ CNTRL, CNTRL | SPACE, CNTRL | SPACE, CNTRL | SPACE,
	CNTRL | SPACE,
/* 13-15 */ CNTRL | SPACE, CNTRL, CNTRL,
/* 16-23 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 24-31 */ CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* space */ PRINT | SPACE,
/* !"#$%&'( */ PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
/* )*+,-./ */ PRINT, PRINT, PRINT, PRINT, PRINT | ALLOW, PRINT | ALLOW,
	PRINT,
/* 012 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 345 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 678 */ PRINT | DIGIT | ALLOW, PRINT | DIGIT | ALLOW,
	PRINT | DIGIT | ALLOW,
/* 9:; */ PRINT | DIGIT | ALLOW, PRINT, PRINT,
/* <=>? */ PRINT, PRINT, PRINT, PRINT,
/* @ */ PRINT,
/* ABC */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* DEF */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* GHI */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* JKL */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* MNO */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* PQR */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* STU */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* VWX */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* YZ[ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT,
/* \]^ */ PRINT, PRINT, PRINT,
/* _`  */ PRINT | ALLOW, PRINT,
/* abc */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* def */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* ghi */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* jkl */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* mno */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* pqr */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* stu */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* vwx */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW,
	PRINT | ALPHA | ALLOW,
/* yz{ */ PRINT | ALPHA | ALLOW, PRINT | ALPHA | ALLOW, PRINT,
/* |}~ */ PRINT, PRINT, PRINT,
/* del */ 0,
/* 80-8f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90-9f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0-af */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0-bf */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0-cf */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0-df */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0-ef */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0-ff */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Free up all resources of an Match entry (including the struct itself).
 * NOTE: this function may (also) be called for Match structs that have only been
 *       setup half-way, so use special care when accessing members (NULL checks!)
 */
void unreal_delete_match(Match *m)
{
	safe_free(m->str);
	if (m->type == MATCH_PCRE_REGEX)
	{
		if (m->ext.pcre2_expr)
			pcre2_code_free(m->ext.pcre2_expr);
	}
	safe_free(m);
}

Match *unreal_create_match(MatchType type, const char *str, char **error)
{
	Match *m = safe_alloc(sizeof(Match));
	static char errorbuf[512];

	*errorbuf = '\0';

	safe_strdup(m->str, str);
	m->type = type;
	
	if (m->type == MATCH_SIMPLE)
	{
		/* Nothing to do */
	}
	else if (m->type == MATCH_PCRE_REGEX)
	{
		int errorcode = 0;
		PCRE2_SIZE erroroffset = 0;
		int options = 0;
		char buf2[512];
		
		if (iConf.spamfilter_utf8)
			options = PCRE2_CASELESS|PCRE2_MATCH_INVALID_UTF;
		else
			options = PCRE2_CASELESS|PCRE2_NEVER_UTF|PCRE2_NEVER_UCP;
		
		m->ext.pcre2_expr = pcre2_compile(str, PCRE2_ZERO_TERMINATED, options, &errorcode, &erroroffset, NULL);
		if (m->ext.pcre2_expr == NULL)
		{
			*buf2 = '\0';
			pcre2_get_error_message(errorcode, buf2, sizeof(buf2));
			if (error)
			{
				if (erroroffset > 0)
					snprintf(errorbuf, sizeof(errorbuf), "%s (at character #%d)", buf2, (int)erroroffset);
				else
					strlcpy(errorbuf, buf2, sizeof(errorbuf));
				*error = errorbuf;
			}
			unreal_delete_match(m);
			return NULL;
		}
		pcre2_jit_compile(m->ext.pcre2_expr, PCRE2_JIT_COMPLETE);
		return m;
	} else if (m->type == MATCH_NONE)
	{
		/* Nothing to do */
	}
	else {
		/* Unknown type, how did that happen ? */
		unreal_delete_match(m);
		return NULL;
	}
	return m;
}

/** Try to match an Match entry ('m') against a string ('str').
 * @returns 1 if matched, 0 if not.
 * @note These (more logical) return values are opposite to the match_simple() function.
 */
int unreal_match(Match *m, const char *str)
{
	if (m->type == MATCH_SIMPLE)
	{
		if (match_simple(m->str, str))
			return 1;
		return 0;
	}
	
	if (m->type == MATCH_PCRE_REGEX)
	{
		pcre2_match_data *md = pcre2_match_data_create(9, NULL);
		int ret;
		
		ret = pcre2_match(m->ext.pcre2_expr, str, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL); /* run the regex */
		pcre2_match_data_free(md); /* yeah, we never use it. unfortunately argument must be non-NULL for pcre2_match() */
		
		if (ret > 0)
			return 1; /* MATCH */		
		return 0; /* NO MATCH */
	}

	if (m->type == MATCH_NONE)
		return 1;

	return 0;
}

int unreal_match_method_strtoval(const char *str)
{
	if (!strcmp(str, "regex") || !strcmp(str, "pcre"))
		return MATCH_PCRE_REGEX;
	if (!strcmp(str, "simple") || !strcmp(str, "glob"))
		return MATCH_SIMPLE;
	if (!strcmp(str, "none"))
		return MATCH_NONE;
	return 0;
}

char *unreal_match_method_valtostr(int val)
{
	if (val == MATCH_PCRE_REGEX)
		return "regex";
	if (val == MATCH_SIMPLE)
		return "simple";
	if (val == MATCH_NONE)
		return "none";

	return "unknown";
}

/* It is unfortunately that we have 2 matching/replace systems.
 * However, the above is for spamfilter matching and stuff
 * and below is for matching on WORDS, which does specific things
 * like replacement on word boundaries etc.
 * Moved here from the censor channel and user mode module
 * (previously was present in both modules, code duplication)
 */
int fast_badword_match(ConfigItem_badword *badword, const char *line)
{
	const char *p;
	int bwlen = strlen(badword->word);

	if ((badword->type & BADW_TYPE_FAST_L) && (badword->type & BADW_TYPE_FAST_R))
		return (our_strcasestr(line, badword->word) ? 1 : 0);

	p = line;
	while((p = our_strcasestr(p, badword->word)))
	{
		if (!(badword->type & BADW_TYPE_FAST_L))
		{
			if ((p != line) && !iswseperator(*(p - 1))) /* aaBLA but no *BLA */
				goto next;
		}
		if (!(badword->type & BADW_TYPE_FAST_R))
		{
			if (!iswseperator(*(p + bwlen)))  /* BLAaa but no BLA* */
				goto next;
		}
		/* Looks like it matched */
		return 1;
next:
		p += bwlen;
	}
	return 0;
}

/* fast_badword_replace:
 * A fast replace routine written by Syzop used for replacing badwords.
 * This searches in line for the bad word and replaces it.
 * buf is used for the result and max is sizeof(buf).
 * Assumptions[!]: max > 0 AND max > strlen(line)+1
 */
int fast_badword_replace(ConfigItem_badword *badword, const char *line, char *buf, int max)
{
	/* Some aliases ;P */
	char *replacew = badword->replace ? badword->replace : REPLACEWORD;
	const char *pold = line; /* pointer to the old string */
	const char *poldx = line;
	char *pnew = buf; /* pointer to the new string */
	int replacen = -1; /* Only calculated if needed. w00t! saves us a few nanosecs? lol */
	int searchn = -1;
	const char *startw, *endw; /* start and end of the word */
	char *c_eol = buf + max - 1; /* Cached end of (new) line */
	int run = 1;
	int cleaned = 0;

	while(run) {
		pold = our_strcasestr(pold, badword->word);
		if (!pold)
			break;
		if (replacen == -1)
			replacen = strlen(replacew);
		if (searchn == -1)
			searchn = strlen(badword->word);
		/* Hunt for start of word */
		if (pold > line) {
			for (startw = pold; (!iswseperator(*startw) && (startw != line)); startw--);
			if (iswseperator(*startw))
				startw++; /* Don't point at the space/seperator but at the word! */
		} else {
			startw = pold;
		}

		if (!(badword->type & BADW_TYPE_FAST_L) && (pold != startw)) {
			/* not matched */
			pold++;
			continue;
		}

		/* Hunt for end of word
		 * Fix for bug #4909: word will be at least 'searchn' long so we can skip
		 * 'searchn' bytes and avoid stopping half-way the badword.
		 */
		for (endw = pold+searchn; ((*endw != '\0') && (!iswseperator(*endw))); endw++);

		if (!(badword->type & BADW_TYPE_FAST_R) && (pold+searchn != endw)) {
			/* not matched */
			pold++;
			continue;
		}

		cleaned = 1; /* still too soon? Syzop/20050227 */

		/* Do we have any not-copied-yet data? */
		if (poldx != startw) {
			int tmp_n = startw - poldx;
			if (pnew + tmp_n >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, poldx, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}

			memcpy(pnew, poldx, tmp_n);
			pnew += tmp_n;
		}
		/* Now update the word in buf (pnew is now something like startw-in-new-buffer */

		if (replacen) {
			if ((pnew + replacen) >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, replacew, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}
			memcpy(pnew, replacew, replacen);
			pnew += replacen;
		}
		poldx = pold = endw;
	}
	/* Copy the last part */
	if (*poldx) {
		strncpy(pnew, poldx, c_eol - pnew);
		*(c_eol) = '\0';
	} else {
		*pnew = '\0';
	}
	return cleaned;
}

/*
 * Returns a string, which has been filtered by the words loaded via
 * the loadbadwords() function.  It's primary use is to filter swearing
 * in both private and public messages
 */
const char *stripbadwords(const char *str, ConfigItem_badword *start_bw, int *blocked)
{
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	int matchlen, m, stringlen, cleaned;
	ConfigItem_badword *this_word;

	*blocked = 0;

	if (!start_bw)
		return str;

	/*
	 * work on a copy
	 */
	stringlen = strlcpy(cleanstr, StripControlCodes(str), sizeof cleanstr);
	matchlen = 0;
	buf[0] = '\0';
	cleaned = 0;

	for (this_word = start_bw; this_word; this_word = this_word->next)
	{
		if (this_word->type & BADW_TYPE_FAST)
		{
			if (this_word->action == BADWORD_BLOCK)
			{
				if (fast_badword_match(this_word, cleanstr))
				{
					*blocked = 1;
					return NULL;
				}
			}
			else
			{
				int n;
				/* fast_badword_replace() does size checking so we can use 512 here instead of 4096 */
				n = fast_badword_replace(this_word, cleanstr, buf, 512);
				if (!cleaned && n)
					cleaned = n;
				strcpy(cleanstr, buf);
				memset(buf, 0, sizeof(buf)); /* regexp likes this somehow */
			}
		} else
		if (this_word->type & BADW_TYPE_REGEX)
		{
			if (this_word->action == BADWORD_BLOCK)
			{
				pcre2_match_data *md = pcre2_match_data_create(9, NULL);
				int ret;

				ret = pcre2_match(this_word->pcre2_expr, cleanstr, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL); /* run the regex */
				pcre2_match_data_free(md); /* yeah, we never use it. unfortunately argument must be non-NULL for pcre2_match() */
				if (ret > 0)
				{
					*blocked = 1;
					return NULL;
				}
			}
			else
			{
				pcre2_match_data *md;
				int ret;
				PCRE2_SIZE *dd;
				int start, end;

				ptr = cleanstr; /* set pointer to start of string */
				while(1) {
					md = pcre2_match_data_create(9, NULL);
					/* ^^ we need to free 'md' in ALL circumstances.
					 * remember this if you break or continue in this loop!
					 */
					ret = pcre2_match(this_word->pcre2_expr, ptr, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL); /* run the regex */
					if (ret > 0)
					{
						dd = pcre2_get_ovector_pointer(md);
						start = (int)dd[0];
						end = (int)dd[1];
						if ((start < 0) || (end < 0) || (start > strlen(ptr)) || (end > strlen(ptr)+1))
						{
							unreal_log(ULOG_FATAL, "main", "BUG_STRIPBADWORDS_PCRE2_MATCH_OOB", NULL,
							           "[BUG] pcre2_match() returned an ovector with OOB start/end: $start/$end, len $length: '$buf'",
							           log_data_integer("start", start),
							           log_data_integer("end", end),
							           log_data_integer("length", strlen(ptr)),
							           log_data_string("buf", ptr));
							abort();
						}
						m = end - start;
						if (m == 0)
						{
							pcre2_match_data_free(md);
							break; /* anti-loop */
						}
						cleaned = 1;
						matchlen += m;
						strlncat(buf, ptr, sizeof buf, start);
						if (this_word->replace)
							strlcat(buf, this_word->replace, sizeof buf); 
						else
							strlcat(buf, REPLACEWORD, sizeof buf);
						ptr += end; /* Set pointer after the match pos */
						pcre2_match_data_free(md);
						continue; /* next! */
					}
					pcre2_match_data_free(md);
					break; /* NOMATCH: we are done! */
				}
				/* All the better to eat you with! */
				strlcat(buf, ptr, sizeof buf);	
				memcpy(cleanstr, buf, sizeof cleanstr);
				memset(buf, 0, sizeof(buf));
				if (matchlen == stringlen)
					break;
			}
		}
	}

	cleanstr[511] = '\0'; /* cutoff, just to be sure */

	return (cleaned) ? cleanstr : str;
}

/** Checks if the specified regex (or fast badwords) is valid.
 * returns NULL in case of success [!],
 * pointer to buffer with error message otherwise
 * if check_broadness is 1, the function will attempt to determine
 * if the given regex string is too broad (i.e. matches everything)
 */
const char *badword_config_check_regex(const char *str, int fastsupport, int check_broadness)
{
	int regex=0;
	const char *tmp;
	static char errorbuf[512];

	if (fastsupport)
	{
		for (tmp = str; *tmp; tmp++) {
			if (!isalnum(*tmp) && !(*tmp >= 128)) {
				if ((str == tmp) && (*tmp == '*'))
					continue;
				if ((*(tmp + 1) == '\0') && (*tmp == '*'))
					continue;
				regex = 1;
				break;
			}
		}
	}
	if (!fastsupport || regex)
	{
		int errorcode = 0;
		PCRE2_SIZE erroroffset = 0;
		pcre2_code *expr;
		int options = 0;
		char buf2[512];

		options = PCRE2_CASELESS|PCRE2_NEVER_UTF|PCRE2_NEVER_UCP;

		expr = pcre2_compile(str, PCRE2_ZERO_TERMINATED, options, &errorcode, &erroroffset, NULL);
		if (expr == NULL)
		{
			pcre2_get_error_message(errorcode, buf2, sizeof(buf2));
			if (erroroffset > 0)
				snprintf(errorbuf, sizeof(errorbuf), "%s (at character #%d)", buf2, (int)erroroffset);
			else
				strlcpy(errorbuf, buf2, sizeof(errorbuf));
			return errorbuf;
		}
		pcre2_code_free(expr);
	}
	return NULL;
}

int badword_config_process(ConfigItem_badword *ca, const char *str)
{
	const char *tmp;
	short regex = 0;
	int ast_l = 0, ast_r = 0;

	/* The fast badwords routine can do: "blah" "*blah" "blah*" and "*blah*",
	 * in all other cases use regex.
	 */
	for (tmp = str; *tmp; tmp++) {
		if (!isalnum(*tmp) && !(*tmp >= 128)) {
			if ((str == tmp) && (*tmp == '*')) {
				ast_l = 1; /* Asterisk at the left */
				continue;
			}
			if ((*(tmp + 1) == '\0') && (*tmp == '*')) {
				ast_r = 1; /* Asterisk at the right */
				continue;
			}
			regex = 1;
			break;
		}
	}
	if (regex)
	{
		int errorcode = 0;
		PCRE2_SIZE erroroffset = 0;
		int options = 0;

		ca->type = BADW_TYPE_REGEX;
		safe_strdup(ca->word, str);

		options = PCRE2_CASELESS|PCRE2_NEVER_UTF|PCRE2_NEVER_UCP;

		ca->pcre2_expr = pcre2_compile(str, PCRE2_ZERO_TERMINATED, options, &errorcode, &erroroffset, NULL);
		if (ca->pcre2_expr == NULL)
		{
			/* This cannot happen since badword_config_check_regex()
			 * should be called from config_test on each regex.
			 */
			config_error("badword_config_process(): failed to compile regex '%s', this is impossible!", str);
			abort();
		}
		pcre2_jit_compile(ca->pcre2_expr, PCRE2_JIT_COMPLETE);
	}
	else
	{
		char *tmpw;
		ca->type = BADW_TYPE_FAST;
		ca->word = tmpw = safe_alloc(strlen(str) - ast_l - ast_r + 1);
		/* Copy except for asterisks */
		for (tmp = str; *tmp; tmp++)
			if (*tmp != '*')
				*tmpw++ = *tmp;
		*tmpw = '\0';
		if (ast_l)
			ca->type |= BADW_TYPE_FAST_L;
		if (ast_r)
			ca->type |= BADW_TYPE_FAST_R;
	}

	return 1;
}

/** Frees a ConfigItem_badword item.
 * Note that it does NOT remove from the list, you need
 * to do this BEFORE calling this function.
 */
void badword_config_free(ConfigItem_badword *e)
{
	safe_free(e->word);
	safe_free(e->replace);
	if (e->pcre2_expr)
		pcre2_code_free(e->pcre2_expr);
	safe_free(e);
}
